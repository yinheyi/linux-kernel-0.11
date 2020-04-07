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
  @brief 该函数主要是对硬盘相关信息的初始化，包含硬盘本身的属性信息(磁头数/磁道数/每磁道的扇区数等),
  硬盘分区信息的设置，偿试加载RAM映像文件，以及挂载根文件系统。
  @param [in] BIOS 该参数在main.c的init()函数中调用时设置为0x90080,这里存放了setup.s程序从
  BIOS读取到的2个硬盘的基本参数表信息(32b).
  @return 成功时返回0， 失败时返回-1.
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
            printk("Unable to read partition table of drive %d\n\r", drive);
            panic("");
        }
        if (bh->b_data[510] != 0x55 || (unsigned char)bh->b_data[511] != 0xAA) {
            printk("Bad partition table on drive %d\n\r", drive);
            panic("");
        }

        p = 0x1BE + (void*)bh->b_data;
        for (i = 0; i < 5; ++i, ++p) {
            hd[i + 5 * drive].start_sect = p->start_sect;
            hd[i + 5 * drive].nr_sects = p->nr_sects;
        }
        brelse(bh);
    }
    if (NR_HD)
        printk("Partition table%s ok. \n\r", NR_HD > 1 ? "s" : "");

    rd_load();      // 偿试加载RAM映像文件至RAM中。
    mount_root();   // 挂载根目录
    return 0;
}


/**
  @brief 判断并循环等待驱动器就绪。
  @return 准备就绪是返回非零值，没有准备就绪时返回0.
  @details 具体为:读控制器状态寄存器端口HD_STATUS(0x1f7), 并检测它的返回值是否等于READY_STAT(0x40),
  它表示驱动器准备好的状态值. 它最大尝试10000次。
  */
static int controller_ready(void) {
    int retries = 10000;
    while (--retries && (inb_p(HD_STATUS) & 0xc0) != 0x40);
    return retries;
}

/**
  @brief 在硬盘执行命令后, 检测硬盘的状态.
  @return 正常时返回0，出错时返回1.
  WRERR_STAT: 驱动器故障.   ERR_STAT：命令执行错误
  */
static int win_result(void) {

    // 读取状态寄存器， 正常情况下，只有READY_STAT(控制器就绪)和SEEK_STAT(寻道结束)两个状态位置位。
    int i = inb_p(HD_STATUS);
    if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT)) == (READY_STAT | SEEK_STAT))
        return 0;

    // 如果有其它错误的话，读取错误状态寄存器至i中，并返回1.
    if (i & 1)
        i = inb(HD_ERROR);
    return 1;
}

/**
  @brief 该函数实现向硬盘控制器中发送命令块, 该函数是与硬盘控制器进行交互的接口。
  @param [in] drive 硬盘号(0或1)
  @param [in] nsect 读取的扇区数
  @param [in] sect  起始的扇区号
  @param [in] head 碰头号
  @param [in] cyl 柱面号
  @param [in] cmd 命令码
  @param [in] intr_addr 硬盘中断处理程序对应的C函数地址。 对硬盘发送不同的命令时，当硬盘执行完毕后,通常会对应不同的处理程序。
  */
static void hd_out(unsigned int drive, unsigned int nsect, unsigned int sect, unsigned int head,
                    unsigned int cyl, unsigned int cmd, void (*intr_addr)(void)) {
    register int port asm("dx");        // port 变量对应了dx寄存器

    // 只支持驱动器0与1，以及磁头号<= 15。
    if (drive > 1 || head > 15)
        panic("Trying to write bad sector");

    // 等待硬盘就绪，如果等待了一会(10000次循环
    if (!controller_ready())
        panic("HD controller not ready.");

    // 该变量在blk.h中通过宏定义, 这里设置新的硬盘中断处理函数. 对于硬盘不同的命令，会对应不同的中断处理函数。
    do_hd = intr_addr;

    /* 从现在开始对硬盘驱动器执行操作. 在对硬盘控制器进行操作控制时，需要同时发送参数和命令。具体参考
       《linux 0.11内核完全注释》(赵炯)第148页。 */
    outb_p(hd_info[drive].ctl, HD_CMD);   // 首先向控制寄存器端口(HD_CMD)发送控制字节,建立起与硬盘的控制方式
    port = HD_DATA;                       // 硬盘控制器数据端口
    outb_p(hd_info[drive].cpcom >> 2, ++port);
    outb_p(nsect, ++port);
    outb_p(sect, ++port);
    outb_p(cyl, ++port);
    outb_p(cyl >> 8, ++port);
    outb_p(0xA0 | (drive << 4) | head, ++port);
    outb_p(cmd, ++port);
}

/** @brief 该函数实现等待一段时间后判断硬盘驱动器是否为busy状态，如果仍然忙返回1，不忙时返回0.  */
static int drive_busy(void) {
    unsigned int i;
    for (i = 0; i < 10000; ++i) {
        if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT | READY_STAT)))
            break;
    }
    i = inb(HD_STATUS);
    i &= BUSY_STAT | READY_STAT | SEEK_STAT;
    if (i == READY_STAT | SEEK_STAT)
        return 0;
    printk("HD controller times out\n\r");
    return 1;
}

/** @brief 执行一下复位硬盘控制器, 检查看看是否存在错误问题, 如果有问题就打印相关信息. */
static void reset_controller(void) {
    int i;

    outb(4, HD_CMD);                        // 向控制寄存器端口发送复位控制字节
    for (i = 0; i < 100; ++i) nop();        // 循环等待一段时间

    outb(hd_info[0].ctl & 0x0f, HD_CMD);    // 发送正常控制字节
    if (drive_busy())
        printk("HD-controller still busy.\n\r");

    // 读错误状态寄存器，如果等于0x01表示无错误，如果不等于0x01表示存在错误，就打印错误值。
    if (i = inb(HD_ERROR) != 1)
        printk("HD-controller reset failed: %02x\n\r", i);
}

/** @brief 该函数实现对硬盘nr的复位操作。 */
static void reset_hd(int nr) {
    // 首先对硬盘控制器进行复位操作
    reset_controller();
    /* 然后向硬盘发送WIN_SPECIFY指令， 并且把硬盘中断函数设置为recal_intr, 意思就是当硬盘执行完WIN_SPECIFY
       指令后，产生中断就去执行recal_intr指令。我有一个疑问， 书上说WINSPECIFY指令不产生中断，如果不产生中断,
       程序如何继续处理request项啊， 所以不对吧？？  */
    hd_out(nr, hd_info[nr].sect, hd_info[nr].sect, hd_info[nr].head - 1, hd_info[nr].cyl, WIN_SPECIFY, &recal_intr);
}

/** @brief 意外硬盘中断处理函数 */
void unexpected_hd_interrupt(void) {
    printk("Unexpected HD interrupt!\n\r");
}

/** @brief 读写硬盘失败的处理函数 */
static void bad_rw_intr(ovid) {
    // 首先增加当前请求项内的错误次数，如果超过了最大错误次数，就终止掉当前请求项
    if (++CURRENT->errors >= MAX_ERRORS)
        end_request(0);
    // 如果出错次数超过了最大错误次数的一半，就对硬盘执行复位硬盘控制器操作
    if (CURRENT->errors > MAX_ERRORS / 2)
        reset = 1;
}

/** @brief 硬盘重新校正中断处理函数 */
static void recal_intr(void) {
    // 首先检测硬盘执行后是否正常，如果不正常，就执行bad_rw_intr()函数.
    if (win_result())
        bad_rw_intr();
    // 继续执行硬盘请求函数
    do_hd_requst();
}


/** @brief 硬盘读操作的中断处理函数。 */
static void read_intr(void) {
    if (win_result()) {
        bad_rw_intr();
        do_hd_requst();
        return;
    }

    port_read(HD_DATA, CURRENT->buffer, 256);
    CURRENT->errors = 0;
    CURRENT->buffer += 512;
    CURRENT->sector++;
    if (--CURRENT->nr_sects) {
        do_hd = &read_intr;
        return;
    }
    end_request(1);
    do_hd_requst();
}

/** @brief 硬盘写操作的中断处理函数。 */
static void write_intr(void) {
    if (win_result()) {
        bad_rw_intr();
        do_hd_requst();
        return;
    }
    if (--CURRENT->nr_sects) {
        CURRENT->sector++;
        CURRENT->buffer += 512;
        do_hd = &write_intr;
        port_write(HD_DATA, CURRENT->buffer, 256);
        return;
    }
    end_request(1);
    do_hd_requst();
}

/** @brief 执行硬盘请求项的函数。
    从下面的代码中可以看出来,读写硬盘操作是异步的， 进程执行了do_hd_requst函数之后就立即返回了，当硬盘执行完了读写操作之后，就
    执行中断处理函数，里面负责处理读/写后的操作。 我们要知道执行读写时, 对应的buffer_head块是加锁的， 只有执行完了读/写操作时， 
    buffer_head块才会解锁。当没有解锁之后，如果有进程要访问buffer_head块，就会进入sleep_on队列中去，在读写完对应的buffer_head块时，
    会焕醒等待该bh块的进程。  */
void do_hd_requst() {
    int i, r;
    unsigned int block, dev;
    unsigned int sec, head, cyl;
    unsigned int nsect;

    INIT_REQUEST;
    dev = MINOR(CURRENT->dev);
    block = CURRENT->sector;        // 起始的扇区号

    if (dev >= 5 * NR_HD || block + 2 > hd[dev].nr_sects) {
        end_request(0);
        goto repeat;
    }
    block += hd[dev].start_sect;
    dev /= 5;
    // 使用汇编语言计算开始的扇区号对应的柱面/磁头/磁道上的扇区号。
    // 开始的扇区号 / 每一磁道的扇区数, 商为磁道数(保存的block中), 余数为磁道上的扇区号(保存的sec中,从0开始计数的)
    // 磁道数 / 磁头数，商为柱面号(保存在cyl中), 余数为第几个磁头号(保存在head中)
    __asm__("divl %4" : "=a"(block), "=d"(sec) : ""(block), "1"(0), "r"(hd_info[dev].sect));
    __asm__("divl %4" : "=a"(cyl), "=d"(head) : ""(block), "1"(0), "r"(hd_info[dev].head));
    sec++;      // 扇区是从1开始计数的,所以这里加上1.
    nsect = CURRENT->nr_sectors;

    if (reset) {
        reset = 0;
        recalibrate = 1;
        reset_hd(CURRENT_DEV);
        return;
    }

    if (recalibrate) {
        recalibrate = 0;
        hd_out(dev, hd_info[CURRENT_DEV].sect, 0, 0, 0, WIN_RESTORE, &recal_intr);
        return;
    }

    if (CURRENT->cmd == WRITE) {
        hd_out(dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr);
        for (i = 0; i < 3000 && !(r = inb_p(HD_STATUS) &  DRQ_STAT); ++i) 
            /* do noting */;
        if (!r) {
            bad_rw_intr();
            goto repeat;
        }
        port_write(HD_DATA, CURRENT->buffer, 256);
    } else if (CURRENT->cmd == READ) {
        hd_out(dev, nsect, sec, head, cyl, WIN_READ, &read_intr);
    } else
        panic("unknown hd-command");
}

/** @brief 硬盘初始化。 */
void hd_init(void) { 
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;      // 设置do_hd_request()函数地址
    set_intr_gate(0x2E, &hd_interrupt);                 // 安装硬盘中断门
    outb_p(inb_p(0x21) & 0xfb, 0x21);                   // 复位8259A int2屏蔽位,允许从片发中断信号。
    outb_p(inb_p(0xA1) & 0xbf, 0xA1);                   // 复位硬盘的中断请求屏蔽位，允许硬盘控制器发送中断请求信号。
}
