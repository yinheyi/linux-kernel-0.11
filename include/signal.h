#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;
typedef unsigned int sigset_t;

#define _NSIG 32                     // 信号的总数
#define NSIG _NSIG

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    6
#define SIGUNUSED 7
#define SIGFPE    8
#define SIGKILL   9                // 强迫进程停止
#define SIGUSR1   10
#define SIGSeGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14              // 定时器报警
#define SIGTERM   15              // 进程终止
#define SIGSTKFLT 16
#define SIGCHLD   17              // 子进程停止或被终止
#define SIGCONT   18              // 恢复进程继续运行
#define SIGSTOP   19              // 进程停止
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22

#define SA_NOCLDSTOP    1
#define SA_NOMASK       0x40000000
#define SA_ONESHOT      0x80000000

#define SIG_BLOCk      0
#define SIG_UNBLOCK    1
#define SIG_SETMASK    2

#define SIG_DEL    ((void(*)(int))0)
#define SIG_IGN    ((void(*)(int))1)

struct sigaction
{
	void (*sa_handler)(int);
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restore)(void);
};


void (*signal(int _sig, void(*_func)(int)))(int);         // 这是一个返回函数指针的函数指针
int raise(int sig);
int kill(pid_t pid, int sig);
int sigaddset(sigset_t* mask, int signo);
int sigdelset(sigset_t* mask, int signo);
int sigemptyset(sigset_t* mask);
int sigfillset(sigset_t* mask);

int sigismember(sigset_t* mask, int signo);
int sigpending(sigset_t* set);
int sigprocmask(int how, sigset_t* set, sigset_t* oldset);
int sigsuspend(sigset_t* sigmask);
int sigaction(int sig, struct sigaction* act, struct sigaction* oldact);

#endif // _SIGNAL_H
