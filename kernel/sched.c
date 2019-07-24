#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>
#define _S(nr) (1 << ((nr) - 1))                        // 把编号为1-32的信号编号转换为信号的位图
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))       // 可阻塞的信号位图

/** @brief 打印给定进程的相关信息，包括进程的id, 进程的状态，进程的内核堆栈中的使用情况。 
* @param [in] nr 进程在进程数组中的索引
* @param [in] p  进程结构体的指针
*
* 从代码中可以看出来，进程的内核堆栈为4kb,里面首先存放了当前进程的struct结构体信息. 
* 疑惑一： struct结构体是怎么什么时候放进去的呢？
* 疑惑二： 这个4kb是在哪指定的呢？
*/
void show_task(int nr, struct task_struct *p)
{
	int i = 0;
	int j = 4096 - sizeof(struct task_struct);        // 进程的内核栈空间为4kb，是在哪定义的呢？

	printk("%d: pid = %d, state = %d, ", nr, p->pid, p->state);
	while (i < j && !((char*)(p + 1))[i])
		++i;
	printk("%d (of %d) chars free in kernel stack\n\r", i, j);
}

/** @brief 该函数调用上面的shor_task()函数把进程数组中的所有进程的信息都显示一遍。
*/
void show_stat(void)
{
	int i;
	for (i = 0; i < NR_TASKS; ++i)
	{
		if (task[i])
			show_task(i, task[i]);
	}
}

#define LATCH (1193180 / HZ)
extern int timer_interrupt(void);
extern int system_call(void);

// 把任务结构成员与stack放在了一个联合体中，看到这里，对上面的疑惑有了一部分的解答。
union task_union
{
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union stask_union init_task = {INIT_TASK};
volatile long jiffies = 0;
long startup_time = 0;               // 开机时间，从1970年1月1日起经过的秒数
struct task_struct* current = &(init_task.task);
struct task_struct* last_task_used_math = NULL;
struct task_struct* task[NR_TASKS] = {&(init_task.task), };
long user_task[PAGE_SIZE >> 2];
struct
{
	long* a;
	short b;
} stack_start = {&user_task[PAGE_SIZE >> 2], 0x10};    // 设置栈的上面定义的user_task数组的尾部。

/** @brief 该函数主要完成数学协处理器的上下文的切换 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;

	__asm__("fwait");

	if (last_task_used_math)
	{
		__asm__("fnsave %0"
				::"m" (last_task_used_math->tss.i387));
	}

	last_task_used_math = current;
	if (current->used_math)
	{
		__asm__("frstore %0"
				::"m" (current->tss.i387));
	}
	else
	{
		__asm__("fninit"::);
		current->used_math = 1;
	}
}

/** @brief 进程调度函数。 */
void schedule(void)
{
	int i, next, c;
	struct task_struct** p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
	{
		if (*p)
		{
			// 检测一下定时器到时的进程, 如果已经到时间，给设置一个信号。
			if ((*p)->alarm && (*p)->alarm < jiffies)
			{
				(*p)->signal |= _S(nr);
				(*p)->alarm = 0;
			}

			// 焕醒一些获得信号的进程, 即把该进程由task_interruptible状态修改为task_running状态。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked))
				&& (*p)->state == TASK_INTERRUPTIBLE)
				(*p)->state = TASK_RUNNING;
		}
	}

	while (1)
	{
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];

		// 该while循环查找进程中counter最大的那一个， 作为接下来要运行的进程
		while (--i)
		{
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
			{
				c = (*p)->counter;
				next = i;
			}
		}
		if (c)
			break;

		// 如果所有的task_running状态的进程的counter都为0了，更新每一个进程的counter值
		// 注意：从代码中可以看出，它更新的是每一个进程的counter值，而不仅仅是task_running的进程。
		// 为什么要这么做呢？
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		{
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
		}
	}
	switch_to(next);
}

/** @brief pause()的系统调用。
*
* 该函数不像看到的那么简单，调用该函数之后，当前进程就会进入睡眠，当前当该进程再一次收到信号被调度时，
* 该函数才会执行return 0 语句。
*/
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

/** @brief 该函数把当前进程设置为不可中断状态, 并且把当前进程插入到睡眠队列的头部
*
* @param [in] p 指向睡眠队列的指针的指针
* 要好好分析一下该函数。
*/
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;
	if (!p)
		return;

	if (current == &(init_task.task))
		panic("task[0] trying to sleep");

	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state = TASK_RUNNING;
}

/** @brief  在点不太明白！
*
*
*/
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;
	if (!p)
		return;

	if (current == &(init_task.task))
		panic("task[0] trying to sleep");

	tmp = *p;
	*p = current;
repeat:
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p!= current)
	{
		(**p).state = TASK_RUNNING;
		goto repeat;
	}
	*p = tmp;
	if (temp)
		tmp->state = 0;
}

/** @brief 有点不太明白，对不对？ */
void wake_up(struct task_struct **p)
{
	if (p && **p)
	{
		(**p).state = TASK_RUNNING;
	}
}

#define TIME_REQUESTS 64        // 最多可以有64个定时器。
// 定时器链表
static struct timer_list
{
	long jiffies;
	void (*fn)();
	struct timer_list* next;
} timer_list[TIME_REQUESTS], *next_timer = NULL;

/** @brief 添加定时器。
*
* @param [in] jiffies 指定的定时值, 应该是相对值
* @param [in] fn 相应的处理函数指针
*/
void add_timer(long jiffies, void(*fn)(void))
{
	struct timer_list *p;
	if (!fn)
		return;

	cli();
	if (jiffies <= 0)
		fn();
	else
	{
		// 从定时器数组中，找一个空的项
		for (p = timer_list; p < timer_list + TIME_REQUESTS; ++p)
		{
			if (!p->fn)
				break;
		}

		if (p >= timer_list + TIME_REQUESTS)
			panic("no more time requests free");

		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;

		// 对链表项的按定时值进行排序，需要知道的是在插入当前定时值之前，链表已经按顺序排好了。
		while (p->next && p->next->jiffies < p->jiffies)
		{
			// 这段代码，省略，因为可能有问题。
			// 从原来的这个代码中，可以看出来，该函数设置的jiffies应该是相对滴答数。
		}
	}
	sti();
}

/** @brief 时钟中断处理程序
*
* @param [in] cpl 当前的特权级
*/
void do_timer(long cpl)
{
	// 与扬声器发声有关
	extern int beepcount;
	extern void sysbeepstop(void);
	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	// 增加用户进程时间或内核进程时间
	if (cpl)
		current->utime++;
	else
		current->stime++;

	// 处理定时器
	if (next_timer)
	{
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0)
		{
			void(*fn)(void);
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			fn();
		}
	}
}

/** @brief 系统调用功能： 设置报警定时的时间值。如果已经设置过，则返回旧值, 否则返回0
 */
int sys_alarm(long seconds)
{
	int old = current->alarm;
	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds > 0) ? (jiffies + HZ * seconds) : 0;
	return old;
}

int sys_getpid(void)
{
	return current->pid;
}

/** @brief 获到父进程的id */
int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

/** @获取组id */
int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

/** @brief 降低对cpu的使用优先权。 */
int sys_nice(long increment)
{
	if (current->priority - increment > 0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct *p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
	set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));

	p = gdt + 2 + FIRST_TSS_ENTRY;
	for (i = 1; i < NR_TASKS; ++i)
	{
		task[i] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}
	__asm__("pushfl; andl $0xffffbfff, (%esp); popfl");
	ltr(0);
	lldt(0);

	outb_p(0x36, 0x43);
	outb_p(LATCH & 0xff, 0x40);
	outb(LATCH >> 8, 0x40);
	set_intr_gate(0x20, &timer_interrupt);
	outb(inb_p(0x21) & ~0x01, 0x21);
	set_system_gate(0x80, &system_call);
}
