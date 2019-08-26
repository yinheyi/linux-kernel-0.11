#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

int sync_dev(int dev);
void wait_for_kerpress(void);

/**
  @brief 检测给定地址的指定位，并返回。
  @return 返回0或1,为int类型。

  汇编指令：  
  - bt指令： bit test，把给定位复制到CF标志位上。
  - setb指令：如果CF位置位，则把操作数(1字节)设置为1,否则设置为0.
  */
#define test_bit(bitnr, addr) ({                          \
register int __res __asm__("ax");                         \
__asm__("bt %2, %3\t\n"                                   \
        "setb %%al"                                       \
        :"=a" (__res)                                     \
        :"a" (0), "r" (bitnr), "m" (*(addr)));            \
__res;})

struct super_block super_blocks[NR_SUPER];     //共 8 项
int ROOT_DEV = 0;

/**
  @brief 锁定指定的超级块。
  @param [in] 指定超级块的指针。
  @return 返回值为空。
  */
static void lock_super(struct super_block* sb)
{
    cli();
    while (sb->s_lock)
        sleep_on(&(sb->s_wait));
    sb->s_lock = 1;
    sti();
}

/**
  @brief 解锁超级块
  @param [in] 指定的超级块
  @return 返回值为空。
  */
static void unlock_super(struct super_block* sb)
{
    cli();
    sb->s_lock = 0;
    wait_up(&(sb->s_wait));
    sti();
}

/**
  @brief 等待超级块的解锁
  @param [in] sb 指定的超级块指针
  @return 返回值为空
  */
static void wait_on_super(struct super_block* sb)
{
    cli();
    while (sb->s_lock)
        sleep_on(&(sb->s_wait));
    sti();
}

/**
  @brief 该函数用于获取指定设备上的超级块，如果不存在，则返回NULL.
  @param [in] dev 设备号
  @return 返回超级块指针.
  
  该函数遍历内存中的超级块数组，查找与指定设备相同的超级块。
  */
struct super_block* get_super(int dev)
{
    struct super_block* s;
    if (!dev)
        return NULL;
    s = 0 + super_blocks;
    while (s < NR_SUPER + super_blocks)
    {
        if (s->s_dev == dev)
        {
            wait_on_super(s);
            if (s->s_dev == dev)
                return s;
            s = super_blocks + 0;
        }
        else
            ++s;
    }
    return NULL;
}

/**
  @brief
  @param
  @return
  */
void put_super(int dev)
{
    struct super_block* sb;
    struct m_inode* inode;
    int i;
    
    if (dev == ROOT_DEV)
    {
        printk("root diskette changed: prepare for armageddon\n\r");
        return;
    }
    if (!(sb = get_super(dev)))
        return;
    if (sb->s_imount)
    {
        printk("mounted disk changed -tssk, tssk\n\r");
        return;
    }
    
    lock_super(sb);
    sb->s_dev = 0;
    for (i = 0; i < I_MAP_SLOTS; ++i)
        brelse(sb->s_imap[i]);
    for (i = 0; i < Z_MAP_SLOT; ++i)
        brelse(sb->s_zmap[i]);
    free_super(sb);
    return;
    }
    }
}
