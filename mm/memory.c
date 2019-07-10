/** \fn memory.c
*   \brief 内存管理相关.
*
* 该文文件主要负责linux下的内存管理机制，它采用了分布管理的方式。利用页目录项和页表项进内存进行申请和释放。
* 每一个内存页占4kb的内存空间。
* 如果想看明白该文件内的代码，需要对x86的保护模式很了解。读下面代码时，时时刻刻要识别线性地址还是物理地址，
*/

#include <signal.h>
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

volatile void do_exit(long code);
static inline volatile void oom(void)
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

/* 该宏的作用就是把cr3寄存器的值设置为0, 作用是什么呢？？
*  猜测可能的原因：每当CR3寄存器重新加载时，处理器都会刷新高速缓冲区
*/
#define invalidate()                         \
__asm__("movl %%eax, %%cr3"                  \
       :                                     \
       :"a" (0));

// 与主存相关的一些宏定义
#define LOW_MEM 0x100000
#define PAGING_MEMORY (15 * 1024 * 1024)
#define PAGING_PAGES (PAGING_MEMORY >> 12)
#define MAP_NR(addr) (((addr) - LOW_MEM) >> 12)
#define USED 100
#define CODE_SPACE(addr) ((((addr) + 0xfff) & ~0xfff) < current->start_code + current->end_code)
static long HIGH_MEMORY = 0;

// 该宏的作用就是使用汇编语言进行内存的的复制
#define copy_page(from, to)                 \
__asm__("cld; rep;  movsl"                  \
       :                                    \
       :"S" (from), "D"(to), "c" (1024)     \
       :"cx", "di", "si");

static unsigned char mem_map[PAGING_PAGES] = {0, };

// 下面函数的作用就是通过查找mem_map内的值，从后向前来找到一个空的内存页。
unsigned long get_free_page(void)
{
	// 如果要读懂下面段汇编代码，你需要了解相关指令，可以看一下该目录下的readme.md文档
	register unsigned long __res asm("ax");
	__asm__("std; repne; scasb\n\t"    // 从后向前比较mem_map的值，如果值不等于eax的值(即不等于0) 或者 没有比较项了，就停止比较
			"jne 1f\n\t"               // 向前跳到标号1处执行。
			"movb $1, 1(%%edi)\n\t"    // 这里之所以在偏移地址edi的基础上加1是因为在执行scasb指令时进行了减1操作。
			"sall $12, %%ecx\n\t"      // ecx的值此时为free_page的index, 把乘以4kb之后，就是从内存低端地址(1M)开始的偏移值。
			"addl %2, %%ecx\n\t"	   // 这里 %2 表示从输出开始算起的第2个寄存器(从第0个开始计数)，即"i" (LOW_MEM),所以%2内的值为低端地址的值。
			"movl %ecx, %%edx\n\t"     // 把实际的物理地址放到edx寄存器中。
			"movl $1024, %%ecx\n\t"    // 下面三行汇编语言的作用是把查找到的内存页进行清零处理。
			"leal 4092(%e%dx), %%edi\n\t"
			"rep;stosl\n\t"
			"movl %%edx, %%eax\n"     // 把物理地址的返回值放到eax寄存器中。
			"1:"
			:"=a" (__res)            // eax寄存器的值赋值给了__res
			:"" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),"D" (mem_map + PAGING_PAGES - 1)
			:"di", "cx", "dx");
	return __res;
}

// 释放一个内存页，操作很简单，直接把mem_map中的值减1即可。
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM)
		return;
	if (addr >= HIGH_MEMORY)
		panic("trying to free nonexistent page");

	// 求出addr对应的内存页的索引。
	addr -= LOW_MEM;
	addr >>= 12;

	if (mem_map[addr]--)		// 注意，--操作会在判断完if语句之后再进行减1
		return;

	mem_map[addr] = 0;
	panic("trying to free free page");
}

/** \brief 功能：从指向的内存地址处释放给定大小的内存空间。
* @param [in] from 给定的逻辑地址, 要求必须4M对齐, 因为每一个页表对应的物理内存的大小就是4M
* @param [in] size 给定的字节大小。
* @return int类型 成功时返回0.
*
* 具体操作就是通过给定的地址在页目录找到对应的页目录项(里面有页表地址)，然后页表内的所有页表项
* 进行free_page操作。
*/
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long* pg_table;
	unsigned long *dir;
	unsigned long nr;

	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("trying to free up swapper memory space");

	size = (size + 0x3fffff) >> 22;		// size的值为4M的进位整数位。
	dir = (unsigned long*) ((from>>20) & 0xffc);
	for ( ; size>0; --size, ++dir)
	{
		// 页目录项的第0位为p位，表示存在位，如果为0表示不存在。
		if (!(1 & *dir))
			continue;

		// 从页目录项中拿到页表地址(页表地址的低12位为0）
		pg_table = (unsigned long*) (0xfffff000 & *dir);
		for (nr = 0; nr < 1024; ++nr)
		{
			if (1 & *pg_table)
				free_page(0xfffff000 & *pg_table);
			*pg_table = 0;
			++pg_table;
		}

		free_page(0xffff000 & *dir);
		*dir = 0;
	}
	invalidate();
	return 0;
}

/** \brief 内存页的拷贝操作，实现写时复制。
* @param [in] from 源地址
* @param [in] to 目录地址
* @param [in] size 字节大小
* 
* 为了实现写时复制的功能，当拷贝内存页时，只拷贝相应的页表项，不会进行物理内存的分配动作。
* 为新的地址申请一个新的内存页用当作页表并把新页表的地址加到页目录中，然后再源地址的页表
* 内容设置为只读后，再拷贝一份放到新建的页表中。
*/
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long* from_page_table;    // 源地址对应页表的地址
	unsigned long* to_page_table;
	unsigned long this_page;
	unsigned long* from_dir;          // 源地址对应的页目录项的地址
	unsigned long* to_dir;
	unsigned long nr;

	if ((from & 0x3fffff) || (to & 0x3fffff))
		panic("copy_page_tables called with wrong alignment");

	from_dir = (unsigned long*)((from >> 20) & 0xffc);
	to_dir = (unsigned long*)((to >> 20) & 0xffc);
	size = (unsigned)(size + 0x3fffff) >> 22;

	for ( ; size-- > 0; ++from_dir, ++to_dir)
	{
		if (!(1 & *from_dir))
			continue;
		if (1 & *to_dir)
			panic("copy_page_tables: already exist");

		from_page_table = (unsigned long*)(*from_dir & 0xfffff000);
		if (!(to_page_table = (unsigned long*)get_free_page()))
			return -1;
		*to_dir = to_page_table | 7;
		nr = (from == 0) ? 0xA0 : 1024;		// 这里特殊处理：第一个调用fork时，只复制160个页就ok了。

		for ( ; nr-- > 0; ++from_page_table, ++to_page_table)
		{
			this_page = *from_page_table;
			if (!(1 & this_page))
				continue;
			this_page &= ~2;		// 设置一些相关的标志位,例如只读
			*to_page_table = this_page;

			// 把源页设置为共享的，即只读。
			if (this_page > LOW_MEM)
			{
				*from_page_table = this_page;
				this_page -= LOW_MEM;
				this_page >>= 12;
				++mem_map[this_page];
			}
		}
	}
	invalidate();
	return 0;
}

/** \brief 功能： 把给定的一个内存页映射到给定的线性地址中.
* @param [in] page 内存页的地址，即该内存页真实的物理地址
* @param [in] adderss 线性地址，即要使用该内存页表示的地址。
* @return unsigned long 返回物理地址。
*
* 通过线性地址找到对应的页表项，把给定的内存页的物理地址放到页表页中即可。
* 在查找页表项的过程中，如果发现不存在页表，那就新建一个页表。
*/
unsigned long put_page(unsigned long page, unsigned long address)
{
	unsigned long temp;
	unsigned long* page_table;

	if (page < LOW_MEM || page > HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[(page - LOW_MEM) >> 12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);

	page_table  = (unsigned long*)(0xffc & (address >> 20));	// 页目录项的地址
	if (*page_table & 1)
		page_table = (unsigned long*)(*page_table & 0xfffff000);	// 页表的地址
	else
	{
		if (!(temp = get_free_page()))
			return 0;
		*page_table = temp | 7;
		page_table = (unsigned long*)temp;
	}

	page_table[(address >> 12) & 0x3ff] = page | 7;
	return page;
}

/** \brief 功能：实现写时复制中的真正复制功能.
* @param [in out] table_entry 页表项的线性地址
* @return void 
*
* 对table_entry代表的内存页进行真实的复制一份，把新复制的内存页的物理地址重新写到
* table_entry指向的页表项中，并把原来共享的页表项和新new出页表项都设置为可写的.
*/
void un_wp_page(unsigned long* table_entry)	// un-write protected
{
	unsigned long old_page;
	unsigned long new_page;

	// 从页表项中拿出对应的物理地址
	old_page = 0xfffff000 & *table_entry;

	// 为什么共享的低内存的页，即使没有被其它人使用，也要进行复制呢？
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1)
	{
		*table_entry |= 2;		// 可写
		invalidate();
		return;
	}

	if (!new_page = get_free_page())
		oom();

	if (old_page >= LOW_MEM)
		--mem_map[MAP_NR(old_page)];
	*table_entry = new new_page | 7;
	invalidate();
	copy_page(old_page, new_page);
}

/** \brief 写时复制的页中断异常时, 该函数相当于中断处理程序。
* @param [in] address 引起中断异常的线性地址
* @return void 空
*
* 通过线性地址找到对应的页表项的地址，然后调用un_wp_page函数来处理。
*/
void do_wp_page(unsigned long errro_code, unsigned long address)
{
#if 0
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif 

	un_wp_page((unsigned long*)
			(((address >> 10) & 0xffc) +                     // 页表内的偏移地址 + 页表地址 = 页表项地址
			 (0xfffff000 & *((unsigned long*)                // 页表的物理地址
							 (address >> 20 & 0xffc))));     // 页目录项物理的地址
}

/**
* 写时页面的验证，验证是否可写，如果不可写，则复制页面。
*/
void write_verify(unsigned long address)
{
	unsigned long page = *((unsigned long*)((address >> 20) & 0xffc));        // address对应的页目录项的内容
	if (!(page & 1))
		return;
	
	page &= 0xffff000;
	page += ((address >> 10) & 0xffc);
}
