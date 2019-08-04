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

void do_signal(long signr, long eax, long ebx, long ecx, long edx, long fs, long es. long ds,
		       long eip, long cs, long eflags, unsigned long* esp, long ss)
{
	unsigned long sa_handler;
	long old_eip = eip;
	struct sigaction* sa = current->sigaction + signr - 1;
	int longs;

	sa_handler = (unsigned long)sa->sa_handler;
	if (sa_handler == 1)
		return;
	if (!sa_handler)
	{
		if (signr == SIGCHLD)
			return;
		else
			do_exit(1 << (signr - 1));
	}
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_flags = NULL;
}
