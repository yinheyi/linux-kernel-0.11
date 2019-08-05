#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <signal.h>

volatile void do_exit(int error_code);

/** @brief 获取当前任务的信号屏蔽码
*/
int sys_sgetmask()
{
	return current->blocked;
}

/** @brief 设置信号新的屏蔽码，返回原始的屏蔽码。
*/
int sys_ssetmask(int newmask)
{
	int old = current->blocked;
	current->blocked = newmask & ~(1 << SIGKILL - 1);        // SIGKILL 是不可以被屏蔽的。
	return old;
}

/** 从from处复制sigactin数据到fs段的to处。
*/
static inline void save_old(char* from, char* to)
{
	int i;
	verify_area(to, sizeof(struct sigaction));
	for (i = 0; i < sizeof(struct sigaction); ++i)
	{
		put_fs_byte(*from, to);
		++from;
		++to;
	}
}

/** @brief 把sigaction数据从fs段中的from处复制到to处。
*/
static inline void get_new(char* from, char* to)
{
	int i;
	for (i = 0; i < sizeof(struct sigaction); ++i)
	{
		*to = get_fs_byte(from);
		++to;
		++from;
	}
}

/** @brief  signal系统调用，为指定的信号安装新的信号句柄。
* @param [in] signum 指定的信号
* @param [in] handler 新的信号句柄函数
* @param [in] restorer 
*/
int sys_signal(int signum, long handler, long restorer)
{
	struct sigaction tmp;
	if (signum < 1 || signum > 32 || signum == SIGKILL)
		return -1;
	tmp.sa_handler = (void (*)(int))handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = (void (*)(int))restorer;

	// 通过下面的代码可以看出，在一个任务中，对于每一个信号(共32个信号)都有一个sigaction数据结构
	handler = (long)current->sigaction[signum - 1].sa_handler;
	current->sigaction[signum - 1] = tmp;
	return handler;
}

/** @brief sigaction()系统调用.
*/
int sys_sigaction(int signum, const struct sigaction* action, struct sigaction* oldaction)
{
	struct sigaction tmp;
	if (signum < 1 || signum > 32 || signum == SIGKILL)
		return -1;
	tmp = current->sigaction[signum - 1];
	get_new((char*)action, (char*)(signum - 1 + current->sigaction));      // 这个地方为什么要这么做呢？为什么不能直接使用赋值语句呢？
	if (oldaction)
		save_old((char*)&tmp, (char*)oldaction);    // 为什么要这么做呢？为什么不能直接使用赋值语句呢？

	if (current->sigaction[signum - 1].sa_flags & SA_NOMASK)
		current->sigaction[signum - 1].sa_mask = 0;
	else
		current->sigaction[signum - 1].sa_mask |= (1 << (signum - 1));
	return 0;
}

/** @brief 该函数在系统调用中断处理函数中使用到。它修改了内核栈中的返回地址的值，使它指向了信号处理函数。
* 当程序从内核空间返回到用户空间时，需要从内核栈切换到用户栈，所以呢，在执行信号处理函数时，程序工作在用户态下,
* 它此时使用的是原本属于用户进程的程序的栈。 在信号处理函数执行过程中，可能会破坏原返回地址处程序的寄存器的内容，
* 所以呢，需要在用户栈中保存一个原来的寄存器。
* @attention: 该函数的这些参数（寄存器的值)就是原地址处的寄存器的值，在系统调用中断处理函数中被入栈的。
*/
void do_signal(long signr, long eax, long ebx, long ecx, long edx, long fs, long es. long ds,
		       long eip, long cs, long eflags, unsigned long* esp, long ss)
{
	unsigned long sa_handler;
	long old_eip = eip;
	struct sigaction* sa = current->sigaction + signr - 1;
	int longs;

	sa_handler = (unsigned long)sa->sa_handler;
	if (sa_handler == SIG_DEL)        // 如果信号处理程序为忽略信号处理程序，则返回就可以了， SIG_DEL在signal.h文件中定义。
		return;
	if (!sa_handler)        // sa_handler == 0 时,就是等于SIG_DEL, 它的值就是0, 表示默认的信号处理程序。
	{
		if (signr == SIGCHLD)    // 当子进程终止时， 会给父进程发送SIGCHLD信号, 在这里，父进程选择了忽略SIGCHLD信号。
			return;
		else
			do_exit(1 << (signr - 1));    // 此时，为什么要终止当前进程呢？可能是因为这个信号我处理不了，并且我也不能忽略的原因？？？
	}

	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_flags = NULL;

	// 关键来了， 修改内核栈中的eip的值，指向了信号处理函数的地址
	*(&eip) = sa_handler;       // 对这个代码是不是很疑问？ 1. 为什么取地址后又进行引用呢？网上说是为了防止编译器的优化。
	                            // 为什么要要修改局部变量的值呢？  其实吧，它就是要修改内核栈中的值,是栈上的值哦。。。

	longs = (sa->sa_flags & SA_NOMASK) ? 7 : 8;
	*(&esp) -= longs;

	verify_area(esp, longs * 4);

	tmp_esp = esp;
	put_fs_long((long)sa->sa_restorer, tmp_esp++);
	put_fs_long(signr, tmp_esp++);
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked, tmp_esp++);
	put_fs_long(eax, tmp_esp++);
	put_fs_long(ecx, tmp_esp++);
	put_fs_long(edx, tmp_esp++);
	put_fs_long(eflags, tmp_esp++);
	put_fs_long(old_eip, tmp_esp++);
	current->blocked |= sa->sa_mask;
}
