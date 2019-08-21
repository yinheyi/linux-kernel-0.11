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
  @brief
  @param
  @return
  */
static inline void unlock_inode(struct m_inode* inode)
{
}
