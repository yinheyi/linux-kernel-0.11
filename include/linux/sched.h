#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS - 1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING 0                // 进程正在运行或就绪状态
#define TASK_INTERRUPTIBLE 1          // 进程处于可中断的等待状态
#define TASK_UNINTERRUPTIBLE 2        // 进程处于不可中断的等待状态
#define TASK_ZOMBIE 3                 // 进程处于僵死状态
#define TASK_STOPPED 4                // 进程已经停止。

#ifdef NULL
#define NULL ((void*)0)
#endif

// 声明一些在其它地方定义的函数名
extern int copy_page_tables(unsigned long from, unsigned long to, unsigned long size);
extern int free_page_tables(unsigned long from, unsigned long size);
extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char* str);
extern int tty_write(unsigned minor, char* buf, int count);

// 函数指针的定义
typedef int (*fn_ptr)();

// 数学协处理器的结构, 用于保存进程切换时执行状态信息
struct i387_struct
{
	long cwd;        // control word
	long swd;        // status word
	long twd;        // tag word
	long fip;        // 协处理器的代码指针
	long fcs;        // 协处理器的段寄存器
	long foo;        // 内存操作数的偏移值。
	long fos;        // 内存操作数的段值
	long st_space[20]; 
};

// TSS数据结构, 用于任务切换时保存现场的寄存器的值。
struct tss_struct
{
	long back_link;
	long esp0;
	long ss0;
	long esp1;
	long ss1;
	long esp2;
	long ss2;
	long cr3;
	long eip;
	long eflags;
	long eax, ecx, edx, ebx;
	long esp;
	long ebp;
	long esi;
	long edi;
	long es;
	long cs;
	long ss;
	long ds;
	long fs;
	long gs;
	long ldt;
	long trace_bitmap;
	struct i387_struct i387;    // 上面定义的数学协处理器的数据结构
};

// 进程描述符的数据结构
struct task_struct
{
	long state;
	long counter;                   // 运行时间计数
	long priority;                  // 运行的优先数， 用于进程调度时

	long signal;                    // 信号，每一个比特表示一种信号。
	struct sigaction sigaction[32]; // 与32个信号对应的结构， signal action.
	long blocked;                   // 进程信号的屏蔽码

	int exit_code;
	unsigned long start_code;       // 表示代码段的起始地址。
	unsigned long end_code;         // 注意：它表示的是代码段的长度(字节数）,而不是代码的终止地址。
	unsigned long end_data;         // 注意：它表示的是代码长度 + 数据长度(字节数)
	unsigned long brk;              // 总长度.  brk 是什么英文字母呢？？？
	unsigned long start_stack;      // 栈的开始地址

	long pid;                       // 
	long father;                    // 父进程的pid
	long pgrp;                      // 进程组号
	long session;                   // 会话号
	long leader;                    // 会话首领

	unsigned short uid;             // 用户标志号
	unsigned short euid;            // effective uid
	unsigned short suid;            // saved uid
	unsigned short gid;             // 组标志号
	unsigned short egid;
	unsigned short sgid;

	long alarm;                     // 报警的定时值                   
	long utime;                     // user time, 用户态的运行时间
	long stime;                     // system time, 系统态的运行时间
	long cutime;                    // child user time, 子进程用户态的运行时间
	long cstime;                    // child system time, 子进程系统态的运行时间
	long start_time;                // 进程开始运行时间

	unsigned short used_math;       // 标志，是否使用了数学协处理器。
	int tty;                        // 进程使用的tty的子设备号， -1表示没有使用。

	unsigned short umask;           // 文件创建属性的屏蔽位
	struct m_inode* pwd;
	struct m_inode* root;
	struct m_inode* executable;
	unsigned long close_on_exec;   // 什么东西？
	struct file* filp[NR_OPEN];    // 进程的文件表结构

	struct desc_struct ldt[3];     // 本进程的局部描述符， 0为空，1为代码段，2为数据段和堆栈段
	struct tss_struct tss;
};

// 初始化任务0的数据结构
#define INIT_TASK {                                                            \
	0, 15, 15,                                                                 \
	0, {{},}, 0,                                                               \
	0, 0, 0, 0, 0, 0,                                                          \
	0, -1, 0, 0, 0,                                                            \
	0, 0, 0, 0, 0, 0,                                                          \
	0, 0, 0, 0, 0, 0,                                                          \
	0, -1,                                                                     \
	0022, NULL, NULL, NULL, 0, {NULL,},                                        \
	{{0, 0}, {0x9f, 0xc0fa00}, {0x9f, 0xc0f200},},                             \
	{0, PAGE_SIZE + (long)&init_task, 0x10, 0, 0, 0, 0, (long)&pg_dir,         \
	 0, 0, 0, 0, 0, 0, 0, 0,                                                   \
     0, 0, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,                                 \
	 _LDT(0), 0x80000000,                                                      \
	 {},                                                                       \
	},                                                                         \
}

// 声明一些用于任务调度的全部变量
extern struct task_struct* task[NR_TASKS];        // 保存所有任务中指针数组
extern struct task_struct* last_task_used_math;
extern struct task_struct* current;
extern long volatile jiffies;                    // 滴答数(10ms/ 滴答)
extern long startup_time;                        // 开机时间，从1970.01.01开始计算的(单位为second)

#define CURRENT_TIME (startup_time + jiffies / HZ)  // HZ就是每秒的滴答数, 文件开始定义的，值为100.

// 添加定时器以及进程睡眠与唤醒的相关函数的声明
extern void add_timer(long jiffies, void (*fn)(void));
extern void sleep_on(struct task_struct **p);
extern void interruptible_sleep_on(struct task_struct **p);
extern void wake_up(struct task_struct **p);

/* 要知道的是：在GDT中，第0项为空，第1项为内核代码段描述符，第2项为内核数据段描述符，第3段为系统段描述符，
   第4项为TSS0, 第5项为LDT0, 第6项为TSS1, 第7项为TDT1,....... （每一个任务在GDT中都有两项，一个是TSS描述符，
   一个是LDT描述符。 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)
#define _TSS(n) ((((unsigned long)n) << 4) + (FIRST_TSS_ENTRY << 3))        // 第n个任务的tss在GDT中的偏移地址(从第0个任务开始算起)
#define _LDT(n) ((((unsigned long)n) << 4) + (FIRST_LDT_ENTRY << 3))        // 第n个任务的ldt在GDT中的偏移地址

// 加载第n个任务中任务寄存器
#define ltr(n) __asm__("ltr %%eax" ::"a" (_TSS(n)))
// 加载第n个任务中局部描述符表寄存器
#define lldt(n) __asm__("lldt %%eax" ::"a" (_LDT(n)))

/* 求取当前任务的索引值，即是第几个任务（0,1,2,3，......, 63), 方法是根据当前任务的tss段描述符在gdt中的位置来推断出来。
   还需要注意的是：该宏命令是给n进行赋值操作的。当你在其它地方使用时，如果不知道它是一个宏而误以为是一个函数时，你可能
   会很纳闷，为什么传值还可能修改参数的值呢！ */
#define str(n)                                        \
__asm__("str %%eax\n\t"                               \
		"subl %2, %%eax\n\t"                          \
		"shrl $4, %%eax"                              \
		:"=a" (n)                                     \
		:"a" (0), "i" (FIRST_TSS_ENTRY << 3))

/* switch_to(n)实现在进程调度时的任务之间的切换。 有一些地方目前还是不太明白
   */
#define switch_to(n) {                                \
	struct {long a, b} __tmp;                         \
	__asm__("cmpl %%ecx, _current\n\t"                \
			"je 1f\n\t"                               \
			"movw %%dx, %1\n\t"                       \
			"xchgl %%ecx, _current\n\t"               \
			"ljmp %0\n\t"                             \ //  跳走了，什么时候回来呢？, 进程的上下文又是在哪里保存的呢？
			"cmpl %%ecx, _last_task_used_math\n\t"    \
			"jne 1f\n\t"                              \
			"clts\n"                                  \
			"1:"                                      \
			::"m" (*&__tmp.a), "m" (*&__tmp.b),       \
			"d" (_TSS(n)), "c" ((long)task[n]));      \
}

// 页面地址对准(4kb对齐)
#define PAGE_ALIGN(n) (((n) + 0xfff) & 0xfffff000)

// 把基址装载到描述符中的指定位置。 看下面的代码时，需要了解一个8字节的描述符中每一位的分布情况。
#define _set_base(addr, base)                         \
__asm__("movw %%dx, %0\n\t"                           \
		"rorl $16, %%edx\n\t"                         \
		"movb %%dl, %1\n\t"                           \
		"movb %%dh, %2"                               \
		::"m" (*((addr)+2)), "m" (*((addr) + 4)),     \
		  "m" (*((addr)+7)), "d" (base)               \
		: "dx")

// 把段长值装载到描述符中的指定位置
#define _set_limit(addr, limit)                       \
__asm__("movw %%dx, %0\n\t"                           \
		"rorl $16, %%edx\n\t"                         \
		"movb %1, %%dh\n\t"                           \
		"addb $0xf0, %%dh\n\t"                        \
		"orb %%dh, %%dl\n\t"                          \
		"movb %%dl, %1"                               \
		::"m" (*(addr)), "m" (*((addr)+6)),           \
		  "d" (limit)                                 \
		:"dx")

#define set_base(ldt, base) _set_base(((char*)&(ldt)), base)
#define set_limit(ldt, limit) _set_limit(((char*)&(ldt)), (limit - 1) >> 12)

// 从描述符中取出基地址
#define _get_base(addr) {                            \
unsigned long __base;                                \
__asm__("movb %3, %%dh\n\t"                          \
		"movb %2, %%dl\n\t"                          \
		"shll $16, %%edx\n\t"                        \
		"movw %1, %%dx"                              \
		: "=d" (__base)                              \
		: "m" (*((addr)+2)), "m" (*((addr)+4)),      \
		  "m" (*((addr)+7)));                        \
__base;}

#define get_base(ldt) _get_base((char*)&(ldt))
#define get_limit(segment) {                         \
unsigned long __limit;                               \
__asm__("lsll %1, %0\n\t"                            \
		"incl %0"                                    \
		:"=r" (__limit)                              \
		:"r"(segment));                              \
__limit;                                             \
}

#endif  // _SCHED_H
