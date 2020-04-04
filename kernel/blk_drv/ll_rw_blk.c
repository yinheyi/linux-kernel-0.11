#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

// 定义了request数组, 32项
struct request request[NR_REQURST];
// 定义request的等待进程指针，当request项都被占用时，进程就在此处等待
struct task_struct* wait_for_request = NULL;
/* 定义了块设备的结构，每一个块设备都对应了一个blk_dev_struct项(里面两个
  指针：一个是请求处理函数指针，一个是块设备上的request项指针) */
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
    {NULL, NULL},               // 0 - NULL
    {NULL, NULL},               // 1 - 内存，RAM
    {NULL, NULL},               // 2 - 软驱
    {NULL, NULL},               // 3 - 硬盘
    {NULL, NULL},               // 4 - ttyx设备
    {NULL, NULL},               // 5 - tty设备
    {NULL, NULL}                // 6
};

/** @brief 给buffer块上锁. */
static inline void lock_buffer(struct buffer_head* bh) {
    cli();
    while (bh->b_lock)
        sleep_on(&bh->b_wait);
    bh->b_lock = 1;
    sti();
}

/** @brief 给buffer_head块解锁 */
static inline void unlock_buffer(struct buffer_head* bh) {
    if (!bh->b_lock)
        printk("ll_rw_lock.c: buffer not locked. \n\r");
    bh->b_lock = 0;
    wake_up(&bh->b_wait);
}

/**
  @brief 向给定的块设备上添加一个requst请求.
  @param [in] dev 要添加到的块设备指针
  @param [in] req 要添加的request项指针
  @return 返回值为空。
  */
static void add_request(struct blk_dev_struct* dev, struct request* req) {
    struct request* tmp;
    req->next = NULL;

    cli();

    // 这里地方什么清除脏位呢？
    if (req->bh)
        req->bh->b_dirt = 0;

    // 如果请求队列为空，就把新加的请求加入到队列中，并立即执行请求处理函数。
    if (!(tmp = dev->current_request)) {
        dev->current_request = req;
        sti();
        (dev->request_fn)();
        return;
    }

    // 下面是利用电梯算法把当前的requst项加入到合适的位置
    for ( ; tmp->next; tmp = tmp->next) {
        if ((IN_ORDER(tmp, req) || !IN_ORDER(tmp, tmp->next)) &&
            IN_ORDER(req, tmp->next))
            break;
    }
    req->next = tmp->next;
    tmp->next = req;
    sti();
}

/**
  @breif 根据buffer_head内的信息构建一个requst项，并且把它们添加到请求队列内.
  @param [in] major 要请求的的主设备号，其它该参数不是必须的，因为可以从buffer_head内拿到的。
  @param [in] bh buffer_head指针，里面存放了关于请求项的全部信息。
  @return  返回值为空。
  */
static void make_request(int major, int rw, struct buffer_head* bh)
{
    struct request* req;
    int rw_ahead;

    /* 当前的读写操作为预读或预写时: 如果bh块锁定了，就退出就可以了，因为读或写不是必须的. 
       如果没有锁定，就把rw标志位置为READ或WRITE.  */
    if (rw_ahead = (rw == READA || rw == WRITEA)) {
        if (bh->b_lock)
            return;
        rw = rw == READA ? READ : WRITE;
    }

    // 检测读写标志变量是否合法
    if (rw != READ &&　rw != WRITE)
        panic("Bad block dev command, must be R/W/RA/WA");

    lock_buffer(bh);
    /* 如果是写操作，并且buffer块内的数据不是脏的(相对于磁盘来说，意思就是与磁盘的数据是同步的),或
       者如果是读操作，并且buffer块的数据是最新的(相对于处理器的而言，意思就是buffer内存放的是最新的数据)
       这时候就不需要对磁盘进行读写操作了， 直接返回就OK了。 */
    if ((rw == WRITE  && !bh->b_dirt) || (rw == READ) && bh->uptodate) {
        unlock_buffer(bh);
        return;
    }

repeat:
    /* 设置读写操作的操作范围，查找一个空闲的requst项。具体来说，读操作时可以使用全部的32个请求项中空闲的，
       写操作时只能使用前2/3部分, 也就是21个。 */
    req = rw == READ ? (request + NR_REQURST) : (request + NR_REQURST * 2 / 3);
    while (--req >= request)
        if (req->dev < 0)
            break;

    // 如果没有找到时，如果当前的读写请求为预读写的，就直接退出； 否则的话就把当前进程睡眠，等待可用的request项。
    if (req < requst) {
        if (rw_ahead) {
            unlock_buffer(bh);
            return;
        }
        sleep_on(&wait_for_request);
        goto repeat;
    }

    // 构建一个request项
    req->dev = bh->b_dev;
    req->cmd = rw;
    req->errors = 0;
    req->sector = bh->b_blocknr << 1;       // 起始的扇区号
    req->nr_sectors = 2;                    // 要读写的扇区数
    req->buffer = bh->b_data;               // 数据缓冲区
    req->waiting = NULL;                    // 任务等待操作完成的地方
    req->bh = bh;

    // 添加到请求队列中
    add_request(major + blk_dev, req);
}

/**
  @brief 底层读写数据块的接口函数，上层要访问底层的设备时都是通过该函数进行访问。
  @param [in] rw 指明访问的类型，读/预读/写/预写(READ/READA/WRITE/WRITEA)
  @param [in] bh buffer_head指针，里面存放了关于请求的全部信息。
  */
void ll_rw_block(int rw, struct buffer_head* bh) {
    unsigned int major;
    if ((major = MAJOR(bh->b_dev)) >= NR_BLK_DEV || !(blk_dev[major].request_fn)) {
        printk("Trying to read nonexistent block-device \n\r");
        return;
    }
    make_request(major, rw, bh);
}

/**
  @brief 块设备的初始化函数，由初始化程序main.c调用。它主要干的工作是初始化请求项数组，将
  所有的请求项置为空闲项(dev== -1表示为空闲项).
  */
void blk_dev_init(void) {
    int i;
    for (i = 0; i < NR_REQURST; i++) {
        request[i].dev = -1;
        request[i].next = NULL;
    }
}
