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
  @brief 释放给定设备的超级块。
  @param [in] 指定的设备号
  @return 返回值为空。
  
  执行如下动作：  
  1. 判断给定的设备号是否等于ROOT_DEV,如果是，则给定提示信息并返回。
  2. 判断给定的设备是否已经存在内存的超级块数组内，如果不存在(说明没有挂载)，则返回。
  3. 如果该超级块的s_imount为1（表示被挂载了吗？)，就返回。
  4. 把当前设备的超级块的s_dev置为0， 释放超级块存放imap和zmap占用的高速缓存块，然后返回了。
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
    sb->s_dev = 0;       // 把s_dev设置为0就表示它已经被释放了，别人可以使用它的了。
    for (i = 0; i < I_MAP_SLOTS; ++i)
        brelse(sb->s_imap[i]);
    for (i = 0; i < Z_MAP_SLOT; ++i)
        brelse(sb->s_zmap[i]);
    unlock_super(sb);
    return;
}

/**
  @brief 该函数实现把指定设备上的超级块读入到内存中。
  @param  [in] dev 指定设备号。
  @return返回超级块在内存中的地址。
  */
static struct super_block* read_supper(int dev)
{
    struct super_block* sb;
    struct buffer_head* bh;
    int i, block;
    
    if (!dev)
        return NULL;
    check_disk_change(dev);           // 这个函数是做什么的呢？核查磁盘改变了？
    if (s = get_super(dev))           // 如果已经存在了内存的超级块内，直接返回就行了。
        return s;
    
    // 从super_blocks数组中查找一个可用的超级块，并进行初始化。
    for (s = super_blocks; ; s++)
    {
        if (s >= NR_SUPER + super_blocks)
            return NULL;
        if (!s->s_dev)
            break;
    }
    s->s_dev = dev;
    s->s_isup = NULL;        // 这是啥？
    s->s_imount = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    
    // 读取磁盘上的超级块到内存中
    lock_super(s);
    if (!(bh = bread(dev, 1)))
    {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    *((struct d_super_block*)s) = *((struct d_super_block*)bh->data);
    brelse(bh);
    if (s->s_magic != SUPER_MAGIC)        // 验证是否是支持的类型，
    {
        s->s_dev = 0;
        unlock_super(s);
        return NULL;
    }
    
    // 初始化s_imap[]和s_zmap[]数组为NULL.
    for (i = 0; i < I_MAP_SLOTS; ++i) 
        s->s_imap[i] = NULL;
    for (i = 0; i < Z_MAP_SLOTS; ++i)  
        s->s_zmap[i] = NULL;
    
    // 读取imap与zmap对应的blocks到内存的高速缓存区内. 读下面的代码有个疑惑:
    // s_imap_blocks与I_MAP_SLOTS的关系是什么? ?? Z_MAP_SLOTS与s_zmap_blocks的关系又是什么？
    block = 2;
    for (i = 0; i < s->s_imap_blocks; ++i)
    {
        if (s->s_imap[i] = bread(dev ,block))
            block++;
        else
            break;
    }
    for (i = 0; i < s->s_zmap_blocks; ++i)
    {
        if (s->s_zmap[i] = bread(dev, block))
            block++;
        else
            break;
    }
    if (block != 2 + s->s_imap_blocks + s->s_zmap_block)       // 没有读成功时，释放掉缓存块并返回NULL
    {
        for (i = 0; i < I_MAP_SLOTS; ++i)
            brelse(s->s_imap[i]);
        for (i = 0; i < Z_MAP_SLOTS; ++i)
            brelse(s->s_zmap[i]);
        s->s_dev = 0;
        return NULL;
    }
    
    // 由于第一个inode和第一个zone都不使用，所以把它们在map中的标志位置1，防止后续查找到它们。
    s->s_imap[0]->b_data[0] |= 1;
    s->s_zmap[0]->b_data[0] |= 1;
    unlock_super(s);
    return s;
}

/**
  @brief 系统调用函数：卸载磁盘。
  @param [in] 设备名
  @return 如果成功则返回0， 如果失败则返回错误码。
 */
int sys_umount(char* dev_name)
{
    struct m_inode* inode;
    struct super_block* sb;
    int dev;
    
    // 获取设备名对应的设备号。
    if (!inode = namei(dev_name)))
        return -ENOENT;
    dev = inode->i_zone[0];        // 这里面放的是什么内容来？？
    if (!S_ISBLK(inode->i_mode))
    {
        iput(inode);
        return -ENOTBLK;
    }
    iput(inode);
    
    // 检测要卸载的设备是否为根设备，如果是，返回正在忙的错误码
    if (dev == ROOT_DEV)
        return -EBUSY;
    // 检测要卸载的设备是否不存在，如果是，则返回错误码
    if (!(sb = get_super(dev)) || !(sb->s_imount))
        return -ENOENY;
    if (!sb->s_imount->i_mount)
        printk("Mounted inode has i_mount = 0\n");
    // 检测要卸载的设备是否有进程正在使用,如果是，则返回忙
    for (inode = inode_table; inode < inode_table + NR_INODE; ++inode)
    {
        if (inode->i_dev = =dev && inode->i_count)
            return -EBUSY;
    }
    
    // 执行卸卸载相关的动作
    sb->s_imount->i_mount = 0;
    iput(sb->s_imount);
    sb->s_imount = NULL;
    iput(sb->s_isup);
    sb->s_isup = NULL;
    put_super(dev);
    sync_dev(dev);
    return 0;
}

/**
  @brief 系统调用：挂载磁盘
  @param [in] dev_name 要挂载的磁盘名
  @param [in] dir_name 要挂载到的路径名
  @param [in] rw_flag 挂载之后的磁盘的读写标志位,也就是说可以控制整个磁盘只读或读写。
  @return 操作成功返回0, 操作失败返回错误码。
  */
int sys_mount(char* dev_name, char* dir_name, int rw_flag)
{
    struct m_inode* dev_i;
    struct m_inode* dir_i;
    struct super_block* sb;
    int dev;
    
    // 获取设备名对应的设备号
    if (!(dev_i = namei(dev_name)))
        return -ENOENT;
    dev = dev_i->i_zone[0];
    if (!S_ISBLK(dev_i->i_mode))
    {
        iput(dev_i);
        return -EPERM;
    }
    iput(dev_i);
    
    // 获取路径名对应的inode指针
    if (!(dir_i = namei(dir_name)))
        return -ENOENT;
    if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO)
    {
        iput(dir_i);
        return -EBUSY;
    }
    if (!S_ISDIR(dir_i->i_mode))
    {
        iput(dir_i);
        return -EPERM;
    }
    
    // 读取超级块，设置挂载点
    if (!(sb = read_super(dev)))
    {
        iput(dir_i);
        return -EBUSY;
    }
    if (sb->s_imount)      // 表示该设备已经挂载
    {
        iput(dir_i);
        return -EBUSY;
    }
    if (dir_i->i_mount)    // 表示该挂载点已经挂载了其它的设备。
    {
        iput(dir_i);
        return -EPERM;
    }
    sb->s_imount = dir_i;
    dir_i->i_mount = 1;
    dir_i->i_dirt = 1;
    return 0;
}

/**
  @brief 该函数实现挂载根目录。
  @param void 输入参数为空。
  @return 返回值为空。
  */
void mount_root(void)
{
    int i, free;
    struct super_block* sb;
    struct m_inode* mi;
    
    if (32 != sizeof(struct d_inode))
        panic("bad inode size!\n");

    for (i = 0; i < NR_FILE; ++i)
        file_table[i].f_count = 0;
    
    if (MAJOR(ROOT_DEV) == 2)
    {
        printk("Insert root floppy and press ENTER");
        wait_for_keypress();
    }
    
    // 对数组内超级块的初始化。
    for (p = &super_blocks[0]; p < &super_blocks[NR_SUPER]; ++p)
    {
        p->s_dev = 0;
        p->s_lock = 0;
        p->s_wait = NULL;
    }
    
    if (!(p = read_super(ROOT_DEV)))
        panic("Unable to mount root.");
    if (!(mi = iget(ROOT_DEV, ROOT_INO)))
        panic("Unable to read root i-node");
    mi->i_count += 3;
    p->s_isup = mi;
    p->s_imount = mi;
    current->pwd = mi;
    current->root = mi;
    
    // 初始化zmap, 为什么都设置为1呢？统计空闲的zones..
    free = 0;
    i = p->s_nzones;
    while (--i >= 0)
    {
        if (!set_bit(i &8191, p->s_zmap[i>>13]->b_data))
            free++;
    }
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    
    // 初始化imap, 为什么都设置为1呢？统计空闲的inode.
    free = 0;
    i = p->s_ninode + 1;
    while (--i >= 0)
    {
        if (!set_bit(i & 8191, p->s_imap[i >> 13]->b_data))
            free++;
    }
    printk("%d/%d free inode \n\r", free, p->s_ninodes);
}

