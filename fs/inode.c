#include <string.h>
#include <sys/stat.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

struct m_inode inode_table[NR_INODE] = {{0,},};    // 32项

static void read_inode(struct m_inode* inode);
static void write_inode(struct m_inode* inode);

/**
  @brief 如果该inode节点上锁时，让当前进程睡眠，直到该inode解锁。
  @param [in] inode 给定节点的指针
  @return 返回为空。
  */
static inline void wait_on_inode(struct m_inode* inode)
{
    cli();    // clear interrupt, 关中断
    while (inode->i_lock)
        sleep_on(&inode->i_wait);
    sti();    // set interrupt 开中断
}

/**
  @brief 对指定的inode节点进行上锁，如果指定的节点已经上锁，则当前进程进入睡眠，等到
  指定的inode节点解锁之后再上锁。
  @param [in] inode 指定的inode节点指针。
  @return 返回为空
  */
static inline void lock_inode(struct m_inode* inode)
{
    cli();
    while (inode->i_lock)
        sleep_on(&inode->i_wait);
    inode->i_lock = 1;
    sti();
}

/**
  @brief 解锁给定的inode节点：首先，把i_lock清零，然后唤醒所有等待该inode的进程。
  @param [in] 输入的inode节点指针
  @return 返回值为空。
  */
static inline void unlock_inode(struct m_inode* inode)
{
    inode->i_lock = 0;
    wake_up(&inode->i_wait);
}

/**
  @brief 该函数实现检测内存中的inode节点是否是已经移除设备上的inode节点，如果是，则把该节点置为无效，即把
  i_dev的值置为0.
  @param [in] dev 已经移除的设备号。
  @return 返回值为空。
  */
void invalidate_inodes(int dev)
{
    int i;
    struct m_inode* inode;
    
    inode = inode_table + 0;
    for (i = 0; i < NR_INODE; ++i, ++inode)
    {
        wait_on_inode(inode);
        if (inode->i_dev == dev)
        {
            if (inode->i_count)
                printk("inode in use on removed disk\n\r");
            inode->i_dev = 0;
            inode->i_dirt = 0;
        }
    }
}
