#ifndef _BLK_H
#define _BLK_H

#define NR_BLK_DEV          7           // 支持的总设备数
#define NR_REQURST          32          // 请求队列的数目

/** @brief 定义了一个磁盘请求的数据结构，该结构内有我们需要的所有信息。*/
struct request {
    int dev;                        // 请求的块设备号
    int cmd;                        // 请求的命令: READ(0)或WRITE(1)
    int errors;                     // 在响应本次请求过程中磁盘产生的错误次数
    unsigned long sector;           // 请求读或写的起始扇区号
    unsigned long nr_sectors;       // 请求读或写的总扇区号数
    char* buffer;                   // 数据缓冲区:要么从里面读数据到磁盘，要么从磁盘读数据写入这里。
    struct task_struct* waiting;    // 
    struct buffer_head* bh;         // 
    struct request* next;           // 下一个请求块指针
};

/** @brief 块设备请求结构 */
struct blk_dev_struct {
    void (*request_fn)(void);           // 
    struct request* current_request;    // 当设备上当前的请求指针
};

/**
  @brief 定义在响应request的优先级, 用于响应磁盘请求时的电梯算法中.
  @param[in] s1,s2 它们的类型是request的指针形式.
 */
#define IN_ORDER(s1, s2)                                    \
    ((s1)->cmd < (s2)->cmd || (s1)->cmd == (s2)->cmd &&     \
     ((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev &&   \
       (s1)->sector < (s2)->sector)))

// 声明了一个数组，每一项对应了一个设备的请求结构项
extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
// 声明了一个数组，每一项存放一个请求项，该数组内的请求项被全部设备共同使用。
extern struct request request[NR_REQURST];
// 声明了一个等待进程的指针，当32个request项都被占用时，进程就要在指针表示的等待队列上进行等待。
extern struct task_struct* wait_for_request;


#ifdef MAJOR_NR                             // 主设备号
#if (MAJOR_NR == 1)                         // 主设备号等于1，表示RAM(虚拟盘)
    #define DEVICE_NAME "ramdisk"
    #define DEVICE_REQUEST do_rd_requst
    #define DEVICE_NR(device) ((device) & 7)
    #define DEVICE_ON(device)
    #define DEVICE_OFF(device)
#elif (MAJOR_NR == 2)                       // 主设备号等于2，表示软盘
    #define DEVICE_NAME "floppy"
    #define DEVICE_INTR do_floppy
    #define DEVICE_REQUEST do_fd_requst
    #define DEVICE_NR(device) ((device) & 3)
    #define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
    #define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))
#elif (MAJOR_NR == 3)                       // 主设备号等于3，表示硬盘
    #define DEVICE_NAME "handdisk"
    #define DEVICE_INTR do_hd
    #define DEVICE_REQUEST do_hd_requst
    #define DEVICE_NR(device) (MINOR(device) / 5)
    #define DEVICE_ON(device)
    #define DEVICE_OFF(device)
#else
    #error "unknown blk device"
#endif


// 定义了当前请求项的指针和当前的设备号
#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

// 这里是定义了一个是函数指针，为什么把变量的定义放到了头文件中的呢？
#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
// 这里是对DEVICE_REQUEST函数的声明
static void (DEVICE_REQUEST)(void);


// extern在此处的作用是特别吗？其实声明或定义函数时，不加extern, 它默认也是extern类型的.
extern inline void unlock_buffer(struct buffer_head* bh)
{
    // 未上锁时的警告
    if (!bh->b_lock)
        printk(DEVICE_NAME": free buffer being unlocked!\n");

    bh->b_lock = 0;
    wake_up(&bh->b_wait);       // 唤醒等待该buffer_head块的进程
}

extern inline void end_request(int uptodate)
{
    DEVICE_OFF(CURRENT->dev);
    if (CURRENT->bh) {
        CURRENT->bh->b_uptodate = uptodate;
        unlock_buffer(CURRENT->bh);
    }
    if (!uptodate) {
        printk(DEVICE_NAME" I/O error.\n\r");
        printk("dev %04x, block %d\n\r", CURRENT->dev, CURRENT->bh->b_blocknr);
    }
    wake_up(&CURRENT->waiting);     // 唤醒等待该请求完成的进程
    wake_up(&wait_for_request);     // 唤醒等待想要一个请求项的进程(一共就32个，没有空闲时进程就等待)
    CURRENT->dev = -1;
    CURRENT = CURRENT->next;
}

#define INIT_REQUEST                                        \
repeat:                                                     \
    if (!CURRENT)                                           \
        return;                                             \
    if (MAJOR(CURRENT->dev) != MAJOR)                       \
        printk(DEVICE_NAME": request list destroyed.");     \
    if (CURRENT->bh) {                                      \
        if (!CURRENT->bh->b_lock)                           \
            panic(DEVICE_NAME": block not locked!");        \
    }
#endif


#endif //#define _BLK_H
