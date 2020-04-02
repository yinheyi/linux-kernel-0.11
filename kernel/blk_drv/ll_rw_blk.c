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
