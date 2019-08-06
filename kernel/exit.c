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
    int err, retval = 0;
    
    if (!pid)
    {
        // 当pid为0时，给当前进程的进程组(该进程组的领头是当前进程)内的所有进程发送消息。
        // 发消息时，privilege参数为1.
        while (--p > &FIRST_TASK)
        {
            if (*p && (*p)->pgrp == current->pid)
                if (err = send_sig(sig, *p, 1))
                    retval = err;
        }
    }
    else if (pid > 0)
    {
       
        
        // 当pid > 0 时，只给ID为pid的进程发送信号。
        // 发消号时，privilege参数为0.
        while (--p > &FIRST_TASK)
        {
            if (*p && (*p)->pid == pid)
                if (err = send(sig, *p, 0))
                    retval = err;
        }
    }
    else if (pid == -1)
    {
        // 当pid为-1时，给所有的进程都发送信号
        // 发消号时，privilege参数为0.
        while (--p > &FIRST_TASK)
        {
            if (err = send_sig(sig, *p, 0))
                retval = err;
        }
    }
    else
    {
        // 当pid为负数并且不等于-1时，给进程组内的进程发送信号。进程组的ID号等于-pid。
        //  发消号时，privilege参数为0.
        while (--p > &FIRST_TASK))
        {
            if (*p && (*p)->pgrp == -pid)
                if (err = send_sig(sig, *p, 0))
                    retval = err;
        }
    }
    return retval;
}

/** @brief 给指定的父进程设置一个SIGCHLD的信号。
* @param [in] pid 指定的父进程pid.
* @return 返回值为空。
*
* 在子进程结束时，会调用该函数给父进程发送一个SIGCHLD的信号。
*/
static void tell_father(int pid)
{
    if (!pid)
        return;
    
    int i;
    for (i = 0; i < NR_TASKS; ++i)
    {
        if (!task[i])
            continue;
        if (task[i]->pid != pid)
            continue;
        task[i]->signal |= (1<<(SIGCHLD - 1));
        return;
    }
    
    printk("BAD, no father\n\r");
    // 把当前进程释放的话， 当前进程占用的内存页怎么办？
    release(current);
}

int do_exit(long code)
{
    int i;
    
    free_page_tables(
}
