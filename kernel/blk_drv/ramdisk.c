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

void rd_load(void) {
    struct buffer_head* bh;
}
