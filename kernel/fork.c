#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long addr);
long last_pid = 0;

/**
* @brief 核实从addrss开始的size个字节都分配了内存页，如果没有分配，则进行分配。
* @param [in] addr 起始地址
* @param [in] size 以字节为单位的空间大小
* @return 返回为空。
*
* 开启了分页机制之后，一个页为4096字节，所以内存每一次也是增加一页。addr的地址很可能没有与页对齐，
* 所以呢，本函数对addr向下取整页，把不够一页的字节大小加到size上，然后调用write_verify()获取对size
* 向上取整的页数。
*/
void verify_area(void* addr, int size)
{
  unsigned long start = (unsigned long)addr;
  size += start & 0xfff;
  start &= 0xfffff000;                    // 此时的start是在进程的数据段内的偏移地址，
  start += get_base(current->ldt[2]);     // 把start 加上 数据段的起始地址，得到了它整个线空间中的地址
  while (size > 0)
  {
    size -= 4096;
    write_verify(start);
    start += 4096;
  }
}

/**
* @brief 该函数实现复制当前进程到目的进程。
* @param [in] nr 进程号
* @param [in] p 目的进程的task_struct 结构体指针
* @return 返回 int 类型，成功时返回0，错误时返回错误码。
*
* 完成了如下任务：
* 1. 为新进程设置start_code 项，设置为局部描述符表中的代码段选择子和数据段选择子。
* 2. 复制了进程的代码段和数据段（其实只是复制了内表而已，实现写时复制功能).
*/
int copy_mem(int nr, struct task_struct *p)
{
    unsigned long old_data_base;
    unsigned long old_code_base;
    unsigned long new_data_base;
    unsigned long new_code_base;
    unsigned long data_limit;
    unsigned long code_limit;
    
    code_limit = get_limit(0x0f);    // 从0x0f选择子中获取段界限，0x0f其实就是局部描述符中的代码段。
    data_limit = get_limit(0x17);    // 从0x17选择子中获取段界限，0x17其实就是局部描述符中的数据段。
    old_code_base = get_base(current->ldt[1]);
    old_data_base = get_base(current->ldt[2]);
    
    if (old_code_base != old_data_base)
        painic(" don't support separate I&D");
    if (data_limit < code_limit)
        panic("bad data_limit");
    
    // 从下面这行代码可以看出来，每一个进程占了64M(0x4000000) 的内存。
    new_data_base = new_code_base = nr * 0x4000000;
    p->start_code = new_code_base;
    set_base(p->ldt[1], new_code_base);
    set_base(p->ldt[2], new_data_base);
    if (copy_page_tables(old_data_base, new_data_base, data_limit))    // copy_page_tables 实现写时复制功能，它只是复制了页表而已。
    {
        free_page_tables(new_data_base, data_limit);
        return -ENOMEM;
    }
    return 0;
}

/**
* @brief 复制一个进程。
*/
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
                 long ebx, long ecx,long edx, long fs, long es, long ds,
                 long eip, long cs, long eflags, long esp, long ss)
{
  struct task_struct *p;
  int i;
  struct file *f;
  
  p = (struct task_struct*)get_free_page();
  if (!p)
    return -EAGAIN;
  
  task[nr] = p;
  *p = *current;
  p->state = TASK_UNINTERRUPTIBLE;
  
  p->pid = last_pid;      // last_pid的值在调用find_empty_process()时更新了。
  p->father = current->pid;
  p->counter = p->priority;
  p->signal = 0;
  p->alarm = 0;
  p->leader = 0;
  p->utime = p->stime = 0;
  p->cutime = p->cstime = 0;
  p->start_time = jiffies;
  p->tss.back_link = 0;
  p->tss.esp0 = PAGE_SIZE + (long)p;
  p->tss.ss0 = 0x10;
  p->tss.eip = eip;
  p->tss.eflags = eflags;
  p->tss.eax = 0;       // 这个就是新进程返回0的原因,因为函数的返回值保存在eax寄存器中
  p->tss.ecx = ecx;
  p->tss.edx = edx;
  p->tss.ebx = ebx;
  p->tss.esp = esp;
  p->tss.ebp = ebp;
  p->tss.esi = esi;
  p->tss.edi = edi;
  p->tss.es = es & 0xffff;
  p->tss.cs = cs & 0xffff;
  p->tss.ss = ss & 0xffff;
  p->tss.ds = ds & 0xffff;
  p->tss.fs = fs & 0xffff;
  p->tss.gs = gs & 0xffff;
  p->tss.ldt = _LDT(nr);
  p->tss.trace_bitmap = 0x80000000;

  if (last_task_used_math == current)
	  __asm__("clts; fnsave %0"::"m"(p->tss.i387));

  if (copy_mem(nr, p))
  {
	  task[nr] = NULL;
	  free_page((long)p);
	  return -EAGAIN;
  }

  // 文件打开次数加1
  for (i = 0; i < NR_OPEN; ++i)
  {
	  if (f=p->filp[i])
		  f->f_count++;
  }

  // 进程的pwd/root/executable目录项的引用次数加1.
  if (current->pwd)
	  current->pwd->i_count++;
  if (current->root)
	  current->root->i_count++;
  if (current->executable)
	  current->executable->i_count++;

  // 设置tss和ldt的描述符项
  set_tss_desc(gdt+(nr << 1) + FIRST_TSS_ENTRY, &(p->tss));
  set_ldt_desc(gdt+(nr << 1) + FIRST_TSS_ENTRY, &(p->ldt));
  p->state = TASK_RUNNING;

  return last_pid;
}

/**
* @brief 下面的函数实现：1. 查找能使用的pid号；2. 查找可以使用的进程在进程数组中的任务号。
*
* 我发现，linus很喜欢使用goto啊。
*/
int find_empty_process(void)
{
	int i;

repeat:
	if ((++last_pid) < 0)
		last_pid = 1;
	for (i = 0; i < NR_TASKS; ++i)
	{
		// 如果发现进程数组中已经有进程使用了last_pid号，
		// 则跳转到repeat处，把last_pid加一， 再一次去验证
		// 是否已经存在进程使用了last_pid号。
		if (task[i] && task[i]->pid == last_pid)
			goto repeat;
	}

	for (i = 1; i < NR_TASKS; ++i)
	{
		if (!task[i])
			return i;
	}
	return -EAGAIN;
}
