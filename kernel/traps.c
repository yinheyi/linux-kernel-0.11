
#include <string.h>
#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

/** \brief 获取给定段给定偏移地址处的一个字节。
* @param [in] seg 段的选择子
* @param [in] addr 段内偏移地址
* @return 返回一个字节值
*/
#define get_seg_byte(seg, addr) (                                       \
regeister char __res;                                                   \
__asm__("push %%fs\t\n"                                                 \
        "mov %%ax, %%fs\t\n"                                            \
        "movb %%fs:%2, %%al\t\n"                                        \
        "pop %%fs"                                                      \
        :"=a" (__res)                                                   \
        :"a" (seg), "m" (*(addr)));                                     \
__res;)

/** \brief 获取给定段给定偏移地址处的4个字节。
* @param [in] seg 段的选择子
* @param [in] addr 段内偏移地址
* @return 返回4个字节值
*/
#define get_seg_long(seg, addr) (                                       \
regeister long __res;                                                   \
__asm__("push %%fs\t\n"                                                 \
        "mov %%ax, %%fs\t\n"                                            \
        "movl %%fs:%2, %%eax\t\n"                                       \
        "pop %%fs"                                                      \
        :"=a" (__res)                                                   \
        :"a" (seg), "m" (*(addr)));                                     \
__res;)

/** \brief 获取fs段选择子 */
#define _fs() (                                                         \
register unsigned short __res;                                          \
__asm__("mov %%fs, %%eax"                                               \
        :"=a" (__res):);                                                \
__res; )

/* 使用汇编语言定义的函数声明，这些函数会调用下面的定义的do开头的异常处理函数。*/
int do_exit(long code);
void page_exception(void);
void divide_error(void);
void debug(void);
void nmi(void);        // 这是什么东西？
void int3(void);       // 与软件调试有关，打断点用
void overflow(void);
void bounds(void);     // 这里什么东西？
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void reserved(void);
void parallel_interrupt(void);
void irq13(void);

/** \brief 中断发生时，打印栈中保存的相关寄存器的值以及当前进程相关信息。
* @param [in] str 需要打印的额外提示信息的字符串
* @param [in] esp_ptr 当前栈顶的指针，使用它从栈中获取寄存器的的值（在进行中断处理时，寄存器的值已经入栈）
* @param [in] nr 错误号
*/
static void die(char* str, long esp_ptr, long nr)
{
        long* esp = (long*)esp_ptr;
        int i;
        
        printk("%s:%04x\n\r", str, nr&0xffff);        // %04x是格式化输出的，表示输出4位16进制数，空的用0补齐。
        printk("EIP:\t%04x:%p\n", esp[1], esp[0]);
        printk("EFLSGS:\t%p\n", esp[2]);
        printk("ESP:\t%04x:%p\n", esp[4], esp[3]);
        printk("fs:%04x\n", _fs());
		printk("base: %p, limit: %p\n", get_base(current->ldt[1]), get_limit(0x17));

		if (esp[4] == 0x17)
		{
			printk("Stack:");
			for (i = 0; i < 4; ++i)
				printk("%p", get_seg_long(0x17, i + (long*)esp[3]));
			printk("\n");
		}

		str(i);                // 该函数是一个宏定义，  在sched.h中定义
		printk("Pid: %d, Process nr: %d\n\r", current->pid, 0xffff & i);
		for (i = 0; i < 10, ++i)
			printk("%02x", 0xff & get_seg_byte(esp[1], i + (char*)esp[0]));
		printk("\n\r");
		do_exit(11);
}


