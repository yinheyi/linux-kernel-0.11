#include <stdafg.h>

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>

extern int end;
struct buffer_head* start_buffer = (struct buffer_head*)&end;
struct buffer_head* hash_table[NR_HASH];
static struct buffer_head* free_list;
static struct task_struct* buffer_wait = NULL;
int NR_BUFFERS = 0;

/**
  @brief 等待指定的缓冲区解锁。
  @param [in] bh 指定的缓冲区的头结构指针
  @return 返回值为空。

  1.当指定的缓冲区被上锁时，进程就进入睡眠状态，等待解锁。  
  2. 进入睡眠之前执行了关中断操作，睡眠之后才进行开中断。。这会不
  会影响中断响应啊？对这个关中断和开中断不明白！  
*/
static inline void wait_on_buffer(struct buffer_head* bh)
{
    cli();
    while (bh->b_lock)
        sleep_on(bh->b_wait);
    sti();
}

/**
  @brief 系统调用，同步设备和内存高速缓冲中的数据。
  @param [in] void 输入参数为空。
  @return 返回int类型，成功返回0.

  1. 首先，把所有的inode写入到调整缓冲区内; (这一步干了什么事，还不清楚!）  
  2. 接着，遍历所有的缓冲块的头结构， 如果有缓冲区的dirty位为1,即
  表示写硬盘不同步，就需要写入到硬盘中。(在遍历缓冲区的过程中，如果
  碰上了缓冲块被上锁了，就等待解锁)  
*/
int sys_sync(void)
{
    int i;
    struct buffer_head* bh;

    sync_inodes();      // 将inode写入到调整缓冲中

    bh = start_buffer;
    for(i = 0; i < NR_BUFFERS; ++i, ++bh)
    {
        wait_on_buffer(bh);
        if (bh->b_dirt)
            ll_rw_block(WRITE, bh);
    }
    return 0;
}

/**
  @brief 对指定设备进行调整缓冲数据与设备上的数据同步操作。
  @param [in] dev 指定的设备号。
  @return 如果成功返回0.

  该函数的实现有一点不明白: 为什么几乎是执行了两遍呢？第一遍没有进行
  sync_inodes()操作，第二遍执行了sync_inodes()操作??? 仅仅执行第二遍不行吗？
*/
int sync_dev(int dev)
{
    int i;
    struct buffer_head* bh;

    bh = start_buffer;
    for (int i = 0; i < NR_BUFFERS; ++i, ++bh)
    {
        if (bh->b_dev != dev)
            continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->dirt)    // 之所以在判断一下dev，是因为wait_on_buffer()可能睡眠。
            ll_rw_block(WRITE, bh);
    }

    // 为什么再重来一遍呢？
    sync_inodes();
    bh = start_buffer;
    for (int i = 0; i < NR_BUFFERS; ++i, ++bh)
    {
        if (bh->b_dev != dev)
            continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->dirt)
            ll_rw_block(WRITE, bh);
    }
}

/**
  @brief 使指定设备在调整缓冲区中的数据无效。
  @param [in] dev 指定的设备。
  @return 返回值为空。

  该函数使用到了struct buffer_head中的b_dev, b_uptodate 和 b_dirt三个值。
  */
void inline invalidate_buffers(int dev)
{
    int i;
    struct buffer_head* bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; ++i, ++bh)
    {
        if (bh->b_dev != dev)
            continue;
        wait_on_buffer(bh);
        if (bh->b_dev == dev)
        {
            bh->b_uptodate = 0;    // 这是什么标志呢/
            bh->b_dirt = 0;
        }
    }
}

/**
  @brief 该程序检测 一个软盘是否已经被更换，如果是，则使高速缓冲中相应的
  缓冲块置为无效状态。
  @param [in] dev 指定的设备号

  1. 首先判断是否为是否为软盘，如果不是则返回。  
  2. 接着判断是否指定的设备，如是不是则返回。  
  3. 接下来,做两件事：  
      - 释放对应设备的i节点位图我逻辑块位图所占的高速缓冲区。
      - 使设备的i节点和数据块信息置为无效。
  */
void check_disk_change(int dev)
{
    int i;
    if (MAJOR(dev) != 2)        // 判断是否为软盘设备.
        return;
    if (!floppy_change(dev & 0x03))    // 判断软盘是否更换.
        return;

    // 释放对应设备的i节点位图我逻辑块位图所占的高速缓冲区。
    for (i = 0; i < NR_SUPER; ++i)
    {
        if (super_block[i].s_dev == dev)
            put_super(super_block[i], s_dev);
    }

    // 使设备的i节点和数据块信息置为无效。
    invalidate_inode(dev);
    invalidate_buffers(dev);
}

#define _hashfn(dev, block) (((unsigned)(dev ^ block) % NR_HASH))      // 哈希值的映射
#define hash(dev, block) hash_table[_hashfn(dev, blok)]                // 获取指定的哈希表中的表项

/**
  @brief 从hash队列和空闲缓冲队列中移走指定的缓冲块。
  @param [in] bh 指定的缓冲块头的指针。
  */
static inline void remove_from_quques(struct buffer_head* bh)
{
    // 从hash队列中移除：
    // 1. 从hash队列中移除指定的buffer_head.
    if (bh->b_next)
        bh->b_next->b_prev = bh_prev;
    if (bh->b_prev)
        bh->b_prev->b_next = b_next;
    // 2. 如果hash列表中的对应项是需要移除的当前项，则让hash项指向下一个缓冲区。
    if (hash(bh->b_dev, bh->b_blocknr) == bh)
        hash(bh->b_dev, bh->b_blocknr) = bh->b_next;

    // 从空闲队列中移除. 
    // 关于下面的代码有一个疑问：如果空闲队列中只有bh项，则bh项的b_prev_free和b_next_pree
    // 都应该指向了自己，此时无法把自己移除掉. 
    // 不确定linus是怎么处理的，他可以加入了一个哨兵节点.
    if (!(bh->b_prev_free) || !(bh->b_next_free))
        panic("free block list corrupted");     // 因为是双向循环链表，所以不会这种情况。
    bh->b_prev_free->b_next_free = bh->b_next_free;
    bh->b_next_free->b_prev_free = bh->b_prev_free;
    if (free_list == bh)
        free_list = bh->b_next_free;
}

/**
  @brief 把指定的缓冲区插入到空闲链表尾部并放放到hash队列中。
  */
static inline void insert_into_queues(struct buffer_head* bh)
{
    // 放入到空闲链表尾部
    bh->b_prev_free = free_list->b_prev_free;
    bh->b_next_free = free_list;
    free_list->b_prev_free->b_next_next = bh;
    free_list->b_prev_free = bh;

    // 放入到hash表中, 如果bh不是设备，则返回.
    bh->b_prev = NULL;
    bh->b_next = NULL;
    if (!bh->b_dev)
        return;
    bh->b_next = hash(bh->b_dev, bh->b_blocknr);
    hash(bh->b_dev, bh->b_blocknr) = bh;
    if (bh->b_next)    // 我认为这里需要判断是否为空的. linus没有判断，我觉得可能有问题！
        bh->b_next->b_prev = bh;
}

/**
  @brief 在高速缓冲中寻找给定设备和指定块的缓冲区块。如果找到则返回相应的缓冲块头的指针，否则返回null.

  大致方法就是首先通过设备号和块号，在hash数组中找到对应的hash值链表，然后遍历链表查找指定的设备号和块号。
  */
static struct buffer_head* find_buffer(int dev, int block)
{
    struct buffer_head* tmp;
    for (tmp = hash(dev, block); temp != NULL; tmp = tmp->b_next)
    {
        if (tmp->b_dev == dev && tmp->b_blocknr == block)
            return tmp;
    }
    return NULL;
}

/**
  @brief 这个函数也是获取指向设备和指定块的缓冲区志的头指针，不明白为什么linus把函数名叫做get_hash_bable()呢？

  该函数在find_buffer()在基础上，得到一个解锁的缓冲区块。
  */
struct buffer_head* get_hash_table(int dev, int block)
{
    struct buffer_head* bh;
    while (1)
    {
        if (!bh = find_buffer(dev, block))
            return NULL;

        bh->b_count++;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_blocknr == block)
            return bh;
        bh->b_count--;
    }
}

#define BADNESS(bh) (((bh)->b_dirt << 1) + (bh)->b_lock)

/**
  @brief 取高速缓冲中指定的缓冲区.
  */
struct buffer_head* getblk(int dev, int block)
{
    struct buffer_head* tmp;
    struct buffer_head* bh;

repeat:
    if (bh = get_hash_table(dev, block))
        return bh;

    // 该do while循环遍历free_list，找到一个最合适的空的缓冲块.
    // 什么是最合适呢？修改位和锁定位都为0,是最合适的。往后继续读代码，你会发现
    // 下面的代码要处理b_dirt或b_block置1的缓冲块。
    tmp = free_list;
    do
    {
        if (tmp->b_count)
            continue;

        if (!bh || BADNESS(tmp) < BADNESS(bh))    // 这行代码多注意一下。
        {
            bh = tmp;
            if (!BADNESS(tmp))
                break;
        }
    } while ((tmp = tmp->b_next_free) != free_list)


    // 如果bh为空，则说明在上面的do-while循环内没有找到空闲的缓冲块，使当前进程睡眠，然后
    // 重新再重复上面的过程。
    if (!bh)
    {
        sleep_on(&buffer_wait);
        goto repeat;
    }

    // 等待缓冲区解锁，解锁之后如果发现又被其它进程占用了，只有回到开始再重复此过程。
    wait_on_buffer(bh);
    if (bh->b_count)
        goto repeat;

    // 当该缓冲块为dirty状态时，，该进程负责对该缓冲块进行数据同步，并再次检测
    while (bh->b_dirt)
    {
        sync_dev(bh->b_dev);
        wait_on_buffer(bh);
        if (bh->b_count)
            goto repeat;
    }

    // 程序走到这里时，因为该进程可能进行了多次睡眠，在睡眠过程中其它进程可能已经把当前
    // 一直检测的缓冲块加入到了已经使用的高速缓存中， 所以再进行检测一遍。
    // 这个地方有点不明白：如果其它进程它该缓冲块加入到了高速缓存中，那么为什么会设置缓冲块
    // 对应的dev和block为当前进程需要查找的dev和block呢？？
    if (find_buffer(dev, block))
        goto repeat;

    // 此时，bh一定是没有被占有，没有上锁，没有被修改的。
    bh->b_count = 1;
    bh->b_dirt = 0;
    bh->b_uptodate = 0;

    // 把当前的缓冲块从hash队列和空闲块链表中移除，设置完dev和block之后，再加入到hash队列和空闲块链表中。
    // 之所以这么做，是为了放到hash表的正确位置。 我认为只需要从hash表中移除再加回去就可以，没有必要
    // 从空闲块链表中移除和加入吧。
    remove_from_queues(bh);
    bh->b_dev = dev;
    bh->b_blocknr = block;
    insert_into_queues(bh);
    return bh;
}

