#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);
int sys_close(int fd);

/**
* @brief 该函数的功能就是释放了一个task_struct 指针指向的内存页，然后再重新调度。
*/
void release(struct task_struct *p)
{
    int i;
    if (!p)
        return;
    
    for (i = 1; i < NR_TASKS; ++i)
    {
        if (task[i] == p)
        {
            task[i] = NULL;
            free_page((long)p);
            schedule();
            return;
        }
    }
    
    panic("trying to release non_existent task");
}

/**
* @brief 给一个指向的进程发送一个信号。
* @param [in] sig 信号
* @param [in] p 进程的task_struct 指针
* @param [in] priv 即privilege, 它给定的执行该函数的权限， 0 表示没有权限。
* @attention 只有当以下情况之一满足时，才能给指定进程设置信号：  
* 1. 调用该函数时，给的priv参数不为零。
* 2. 当前的进程和指定的进程具有相同的euid, 即有效用户ID
* 3. 是超级用户
*/
static inline int send_sig(long sig, struct task_struct* p, int priv)
{
    if (!p || sig < 1 || sig > 32)
        return -EINVAL;
    if (priv || (current->euid == p->euid) || suser())
        p->signal |= (1 << (sig - 1));
    else
        return -EPERM;
    return 0;
}

/** @brief 关闭当前的会话。
* @param [in] void 输入为空。
* @return 返回值为空。
*
* 关闭当前进行的session时， 遍历整个任务数组(除FIRST任务之外）, 如果某个进程的session号和当前进程的session号相同，
* 则给那一个进程设置SIGHUP的信号。
*/
static void kill_session(void)
{
    struct task_struct** p= NR_TASKS + task;
    
    while (--p > &FIRST_TASK)
    {
        if (*p && (*p)->session == current->session)
            (*p)->signal |= 1 << (SIGHUP - 1);
    }
}

/** @brief 为什么这个函数名字是kill呢，它的功能明明是调用函数send_sig()给进程发送信号呢。
* @param [in] pid 进程的ID号
* @param [in] sig 信号
*/
int sys_kill(int pid, int sig)
{
    struct task_struct **p = NR_TASKS + task;
}
