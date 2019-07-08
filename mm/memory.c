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

// 该宏的作用就是把cr3寄存器的值设置为0, 作用是什么呢？？
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


}
