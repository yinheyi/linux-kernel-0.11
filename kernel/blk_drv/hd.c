#include <linux/config.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR 3
#include "blk.h"

/**
  @brief 从CMOS存储器中读取给定地址处的一个字节。

  CMOS的地址空间是在基本的地址空间之外的，宁需要通过端口70h/71h使用IN/OUT指令来访问。
  为了读取指定偏移位置的字节，首先需要使用OUT向端口70h发送指定的偏移地址，然后使用IN指
  令从71h端口处读取指定字节的信息。
 */
#define CMOS_READ(addr) ({          \
        outb_p(0x80 | addr, 0x70);  \
        intb_p(0x71);               \
        })

#define MAX_ERRORS 7                // 定义了读取一个磁盘的扇区时，最大允许的出错次数，超过了该次数后就返回了。
#define MAX_HD  2                   // 系统支持的最多硬盘数目

/** 函数：硬盘中断程序在复位操作时，会调用该函数进行重新校正(re-calibrate-interrupt)。 */
static void recal_intr(void);
static int recalibrate = 1;         // 重新校正的标志，将会把碰头移动到0柱面上

/** @brief 定义了磁盘参数相关的数据结构 */
struct hd_i_struct {
    int head;       // 磁头的个数
    int sect;       // 每一个磁道的扇区数
    int cyl;        // 柱面数
    int wpcom;      // 写前预补偿柱面号
    int lzone;      // 磁疛着陆柱面号
    int ctl;        // 控制字节
};

// 下面定义了两个磁盘参数相关信息的数组。
#ifdef HD_TYPE      // 在include/linux/config.h文件中定义了HD_TYPE
    struct hd_i_struct hd_info[] = {HD_TYPE};
    #define NR_HD ((sizeof(hd_info)) / (sizeof(struct hd_i_struct)))
#else
    struct hd_i_struct hd_info[] = {{0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
    static int NR_HD = 0;
#endif


/**
  @brief 定义了磁盘的分区结构信息，包含起始的扇区号和当前分区的扇区总数。
  @details 说明一个为什么是乘以5： 一个磁盘可以分为四个主分区，hd0表示第1个磁盘，
  hd1表示第一个磁盘的第1个分区，hd2表示第一个磁盘的第2个分区......, hd5表示第2个
  磁盘，hd6表示第二个磁盘的第1个分区，....,hd9表示第二个磁盘的第4个分区
  */
static struct hd_struct {
    long start_sect;
    long nr_sects;
} hd[5 * MAX_HD] = {{0, 0}, };


/**
  @brief 从给定端口处读取nr个字(一个字等于2个字节)到buf处。

  cld指令：把DF位清0， 在执行字符串相关的指令时，会递增si和di的值。
  rep指令：重复执行紧随其后的字符串指令，直到cx == 0时停止,在第一次执行之后都会递减cx的值。
  insw指令：从指定端口内读到一个字的数据到ES:DI处，并且会递增DI的值(当DF位为1时，会递减DI的值)
  "d"(port)表示把port值保存到edx寄存器中
  "D"(buf)表示把buf值保存到了edi寄存器中
  "c"(nr)表示把nr值保存到了ecx寄存器中
  */
#define port_read(port, buf, nr)                \
    __asm__("cld; rep; insw" : :"d"(port), "D"(buf), "c"(nr) : "cx", "di")

/**
  @brief 向给定端口处写入nr个字(一个字等于2个字节).
  @details 与上面读端口的区别是：1. insw指令换成了outsw指令; 2. edi寄存器换成了esi寄存器。
  */
#define port_write(port, buf, nr)
    __asm__("cld; rep; outsw" : :"d"(port), "S"(buf), "c"(nr) : "cx", "si")


extern void hd_interrupt(void);
extern void rd_load(void);

/**
  @brief 
  @param [in] BIOS 该参数在main.c的init()函数中调用时设置为0x90080,这里存放了setup.s程序从
                   BIOS读取到的2个硬盘的基本参数表信息(32b).
  */
int sys_setup(void* BIOS) {
    static int callable = 1;       // 注意，它是静态变量
    int i, drive;
    unsigned char cmos_disks;
    struct partition* p;
    struct buffer_head* bh;

    // 通过callable 变量保证该函数只会被调用一次, 它的初始值为1，调用之后变成了0.
    if (!callable)
        return -1;
    callable = 0;

    // 如果没有定义HD_TYPE变量的话，也就是没有定义磁盘参数信息的话，就从BIOS参数中读取.
#ifndef HD_TYPE
    for (drive = 0; drive < 2; ++drive) {
        hd_info[drive].cyl = *(unsigned short*)BIOS;            // 柱面数
        hd_info[drive].head = *(unsigned char*)(BIOS+2);        // 磁头数
        hd_info[drive].wpcom = *(unsigned short*)(BIOS+5);      // 写前预补偿柱面号
        hd_info[drive].ctl = *(unsigned short*)(BIOS+8);        // 控制字节
        hd_info[drive].lzone= *(unsigned short*)(BIOS+12);      // 磁头着陆区柱面号
        hd_info[drive].sect= *(unsigned char*)(BIOS+14);        // 每一个磁道扇区数
        BIOS += 16;
    }

    /* 在setup.s程序中在取BIOS硬盘表信息时，如果只有一个硬盘，就会将对应的第2个硬盘的16B
       全部清零，所以这里是判断是否存在第2块硬盘.   */
    if (hd_info[1].cyl)
        NR_HD = 2;
    else
        NR_HD = 1;
#endif

    // 设置每一个硬盘的起始扇区号和总扇区数
    for (i = 0; i < NR_HD; ++i) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
    }

    /* 下面是根据CMOS内的信息检测硬盘是不是AT兼容的，细节也不想去了解了. 有几兼容的硬盘，
       就把NR_HD变量设为几, 把不支持的硬盘信息清除掉。 这里有点问题啊，如果定义了HD_TYPE
       宏的话，NR_HD就是一个宏而已，此时变量NR_HD是未定义的状态啊。 */
    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0) {
        if (cmos_disks & 0x0f)
            NR_HD = 2;
        else
            NR_HD = 1;
    }
    else
        NR_HD = 0;
    for (i = NR_HD; i < 2; ++i) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = 0;
    }

    /* 下面读取每一块硬盘上的第0块(硬盘的设备号分别为0x300和0x305),里面存入了分区表的信息。
        根据硬盘头第1个扇区位置0xfe处的两个字节是否为55AA来判断该扇区中位于0x1BE开始的分区
        表是否有效. */
    for (drive = 0; drive < NR_HD; ++drive) {
        if (!(bh = bread(0x300 + drive * 5, 0))) {
        }
    }
}

