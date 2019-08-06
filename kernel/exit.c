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
