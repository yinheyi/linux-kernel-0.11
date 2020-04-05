#include <string.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/memory.h>

#define MAJOR_NR 1
#include "blk.h"

char* rd_start;     // ram盘在内存中的起始地址
int rd_length = 0;  // ram盘的总大小(字节为单位)

/** @brief 执行RAM盘的读写操作函数。 */
void do_rd_request(void) {
    int len;
    char* addr;

    // 在执行请求操作前，先调用INIT_REQUEST宏对request项进行合法性的检测(该宏定义在blk.h文件中)
    INIT_REQUEST;

    // 求要操作的起始扇区对应的内存的地址以及总扇区数对应的内存长度, 一个扇区为512，所以左移9位
    addr = rd_start + (CURRENT->sector << 9);
    len = CURRENT->nr_sectors << 9;

    if (MINOR(current->dev) != 1 || (addr + len > rd_start + rd_length)) {
        end_request(0);
        goto repeat;        // 该repeat在INIT_REQUEST宏内定义的
    }

    if (CURRENT->cmd == WRITE)
        memcpy(addr, CURRENT->buffer, len);
    else if (CURRENT->cmd == READ)
        memcpy(CURRENT->buffer, addr, len);
    else
        panic("unkown ramdisk-command");
    end_request(1);
    goto repeat;
}

/**
  @brief 虚拟盘的初始化函数，它初始化了虚拟盘的在内存中的起始地址/长度/,并清零操作。
  @param [in] mem_start 内存的起始地址
  @param [in] length 虚拟盘的大小
  @return 返回虚拟盘的大小值
  */
long rd_init(long mem_start, int length) {
    int i;
    char* cp;

    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    rd_start = (char*)mem_start;
    rd_length = length;
    cp = rd_start;
    for (i = 0; i < length; ++i)
        *cp++ = '\0';
}

/**
  @brief 该函数偿试从软盘中的第256块处读取虚拟盘的映像文件到虚拟盘中, 如果读取成功，把根文件系统
         设置为虚拟盘。
  */
void rd_load(void) {
    struct buffer_head* bh;
    struct super_block s;
    int block = 256;            // 根文件系统的映像文件在boost盘的第256磁盘块开始处。
    int i = 1;
    int nblocks;
    char* cp;

    // rd_length表示RAM盘的大小
    if (!rd_length)
        return;
    printk("Ram disk: %d bytes, starting at 0x%x\n", rd_length, (int)rd_start);

    // 为什么此时根设备不是软盘的时候，就退出呢？
    if (MAJOR(ROOT_DEV) != 2)
        return;

    /* 软盘从第256磁盘块的地方开始存放着虚拟盘的的映像文件, 第256块是虚拟盘的引导块，第257块是
       虚拟盘根文件系统的超级块, 从超级块中可以知道虚拟盘共有多少个块。 */
    bh = breada(ROOT_DEV, block+1, block, block+2, -1);
    if (!bh) {
        printk("Disk error while looking for ramdisk! \n");
        return;
    }
    *((struct d_super_block*)&s) = *((struct d_super_block*)bh->b_data);
    if (s.s_magic != SUPER_MAGIC)
        return;

    /* 首先从超级块中拿到共有多少个逻辑块的个数， 然后逻辑块数再乘以 2^log_zone_size, 得到了对应的数据块个数,
        (linux0.11中一个数据块等于2个扇区, 数据块的长度等于高速缓冲区块的长度), 然后对比一下在内存中初始化的
        虚拟盘空间能否装得下去RAM的映像文件。 */
    nblocks = s.s_nzones << s.s_log_zone_size;
    if (nblocks > (rd_length >> BLOCK_SIZE_BITS)) {
        printk("Ram disk image too big! (%d blocks, %d avalible)\n", nblocks, rd_length >> BLOCK_SIZE_BITS);
        return;
    }

    printk("Loading %d bytes into ram disk ... 0000k", nblocks << BLOCK_SIZE_BITS);
    cp = rd_start;
    while (nblocks) {
        if (nblocks > 2)
            bh = breada(ROOT_DEV, block, block+1, block+2, -1);
        else
            bh = bread(ROOT_DEV, block);
        if (!bh) {
            printk("I/O error on block %d, aborting load \n");
            return;
        }
        memcpy(cp, bh->b_data, BLOCK_SIZE);
        brelse(bh);
        printk("\010\010\010\010\010 %4dk", i);
        cp + BLOCK_SIZE;
        block++;
        nblocks--;
        i++;
    }
    printk("\010\010\010\010\010 done\n");

    // 读取完之后，把根文件系统修改为虚拟盘
    ROOT_DEV = 0x0101;
}
