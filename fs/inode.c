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

/**
  @brief 该函数实现对内存中节点数组中的inode进行同步到磁盘中去，具体为：检测inode节点的i_dirt位是否置1，如果是，
  则对该inode执行写动作。检测过程中，排除了管道文件。
  @param void 输入参数为空。
  @return 返回值为空。
  */
void sync_inodes(void)
{
  int i;
  struct m_inode* inode;
  inode = 0 + inode_table;
  for (i = 0; i < NR_INODE; ++i, ++inode)
  {
    wait_on_inode(inode);
    if (inode->i_dirt && !inode->i_pipe)
      write_inode(inode);
  }
}

/**
  @brief 该函数的功能是把inode中block的相对索引值映射到磁盘真实的逻辑块号。
  @param [in] inode inode的指针
  @param [in] block 实际data的逻辑块在inode的索引值，从0开始，分别为0, 1, 2, 3, ...N, 
  但是不能大于等于 7 + 512 + 512 * 512.
  @parm [in] create 当要查找到block号在inode中不存在时，是否创建一个逻辑块。
  @return 返回磁盘上的逻辑块号。
  */
static int _bmap(struct m_inode* inode, int block, int create)
{
    struct buffer_head* bh;
    int i;
    
    if (block < 0)
        panic("_bmap: block < 0");
    if (block >= 7 = 512 + 512 * 512)
        panic("_bmap: block > big");
    
    // 当block的值小于7时，可以直接从i_zone中拿到数据对应的逻辑块号。
    if (block < 7)
    {
        if (create && !inode->i_zone[block])
        {
            if (inode->i_zone[block] = new_block(inode->i_dev))
            {
                inode->i_ctime = CURRENT_TIME;
                inode->i_dirt = 1;
            }
        }
        return inode->i_zone[block];
    }
    
  // 当block >= 7 并且< 7 + 512 时， 需要通过一次间接来得到磁盘中的逻辑块号。
    block -= 7;
    if (block < 512)
    {
        if (create && !inode->zone[7])
        {
            if (inode->i_zone[7] = new_block(inode->i_dev))
            {
                inode->i_dirt = 1;
                inode->i_ctime = CURRENT_TIME;
            }
            
            if (!inode->zone[7])
            return 0;
        }
  
        if (!(bh = bread(inode->i_dev, inode->i_zone[7])))
            return 0;
        i = ((unsigned short*)(bh->data))[block];     // 这代码，牛逼！
        if (create && !i)
        {
            if (i = new_block(inode->i_dev))
            {
                ((unsigned short*)(bh->b_data))[block] = i;
                bh->b_dirt = 1;
            }
        }
        brelse(bh);
        return i;
    }
    
    // 这时，需要两次间接查找，才能找到相应的逻辑块号。
    block -= 512;
    if (create && !inode->i_zone[8])
    {
        if (inode->i_zone[8] = new_block(inode->i_dev))
        {
            inode->i_dirt = 1;
            inode->i_ctime = CURRENT_TIME;
        }
        
        if (!inode->i_zone[8])
            return 0;
    }
    
    if (!(bh = bread(inode->i_dev, inode->i_zone[8])))
        return 0;
    
    i = ((unsigned short*)(bh->data))[block>>9];
    if (create && !i)
    {
        if (i = new_block(inode->i_dev))
        {
            ((unsigned short*)(bh->b_data))[block >> 9] = i;
            bh->b_dirt = 1;
        }
        
        brelse(bh);
        if (!i)
            return 0;
    }
    
    if (!(bh = bread(inode->i_dev, i)))
        return 0;
    i = ((unsigned short*)(bh->data))[block & 511];      // 这代码，厉害！
    if (create && !i)
    {
        if (i = new_block(inode->i_dev))
        {
            ((unsigned short*)(bh->data))[block & 511] = i;
            bh->b_dirt = 1;
        }
        brelse(bh);
        return i;
    }
}

/**
  @brief 
  @param [in] inode i节点的指针
  @param [in] block
  @return 
  */
int bmap(struct m_inode* inode, int block)
{
    return _bmap(inode, block, 0);
}

/**
  @brief 
  @param [in] inode i节点的指针
  @param [in] block
  @return 
  */
int create_block(struct m_inode* inode, int block)
{
    return _bmap(inode, block, 1);
}

/**
  @brief
  @param [in] inode 需要释放的i节点的指针。
  @return 返回值为空。
  
  该函数做了以下事情：  
  1. 首先判断inode节点是否有效(i_count不能为0），如果无效则死机！
  2. 如果该inode为管道，如果i_count > 1，则递减引用计数之后返回; 如果i_count == 1, 则释放掉相应的内存空间，返回。
  3. 如果该inode的i_dev为0，递减i_count之后(不在乎原来的i_count的值是多少，反正就是减它)，直接返回。
  4. 如果inode的i_mode是块设备，则执行一个同步动作，这个不太明白i_zone[0]是什么内容。。接下来，查看inode的i_count值，如果
  大于1，则减1之后直接返回了; 如果i_count == 1时，判断i_nlinks是否为0（即没有目录项引用该inode了)，如果i_nlinks == 0，
  则释放inode，返回，如果i_nlinks > 0, 则递减i_count之后，返回了。
  */
void iput(struct m_inode* inode)
{
    if (!inode)
        return;
    
    wait_on_inode(inode);
    if (!inode->i_count)
        panic("iput: trying to free free inode!");
    
    // 管道
    if (inode->i_pipe)
    {
        wait_up(&inode->i_wait);      // 唤醒等待该inode的进程。
        if (--inode->i_count)         // 如果当前inode还有其它进程使用，减少计数，直接返回了。
            return;
        
        free_page(inode->i_size);     // i_size 是什么含义？
        inode->i_count = 0;
        inode->i_dirt = 0;
        inode->i_pipe = 0;
        return;
    }
    
    if (!inode->i_dev)      // 如果inode的设备号为0时，递减引用计数，然后返回。
    {
        inode->i_count--;
        return;
    }
    
    if (S_ISBLK(inode->i_mode))
    {
        sync_dev(inode->i_zone[0]);        // i_zone[0] 里面是什么内容？？？
        wait_on_inode(indoe);              // 执行该语句的目的是什么？是因为sync_dev()可能引起当前睡眠吗？
    }
    
repeat:
    if (inode->i_count > 1)
    {
        inode->i_count--;
        return;
    }
    
    if (!inode->i_nlinks)        // 此时对应inode的i_count == 1, i_nlinks = 0时，
    {
        truncate(inode);        // 这是啥？
        free_inode(inode);
        reteurn;
    }
    if (inode->i_dirt)
    {
        write_inode(inode);
        wait_on_inode(inode);
        goto repeat;
    }
    inode->i_count--;
    return;
}

/**
  @brief 该函数实现从inode_table中查找一个空inode,并把inode的内容清零，只保留i_count= 1,返回inode的指针。
  @param void 参数为空
  @return 返回inode的指针。
  */
struct m_inode* get_empty_inode(void)
{
    struct m_inode* inode;
    static strcut m_inode* last_inode = inode_table;     // 静态变量只会在第一次初始化时进行赋值操作。
    int i;
    
    do
    {
        inode = NULL;
        // 该for循环优先从inode_table中找到一个i_count = 0, i_dirt = 0, i_lock= 0 的inode, 再差一点的话，就
        // 找一个i_count = 0 的inode, 如果都没有满足条件的，那么对不起，接下来就死机了！
        for (i = NR_INODE; i; i--)
        {
            if (++last_inode >= inode_table + NR_INODE)
                last_inode = inode_table;
            
            if (!last_inode->i_count)
            {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock)
                    break;
            }
        }
        
        // 如果从inode_table中没有找到满足条件的indoe,则打印相关信息，并死机！
        if (!inode)
        {
            for (i = 0; i < NR_INODE; ++i)
                printk("%04x: %6d\t", inode_table[i].i_dev, inode_table[i].i_num);
            
            panic("not free inodes in memory");
        }
        
        wait_on_inode(inode);
        while (inode->i_dirt)
        {
            write_inode(inode);
            wait_on_inode(inode);
        }
        
    } while (inode->i_count);
    
    // 把inode内的内容全部清零，只保留了i_count = 1
    memset(inode, 0, sizdof(*inode));
    inode->i_count = 1;
    return inode;
}
