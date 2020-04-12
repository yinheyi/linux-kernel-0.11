#ifndef _TTY_H
#define _TTY_H

#include <termios.h>
#define TTY_BUF_SIZE 1024   // 定义成2的指数

// tty等待队列数据结构
struct tty_queue {
    unsigned long data;     // 等待队列缓冲区中当前字符行数, 对于串行终端，存放品德端口地址
    unsigned long head;     // head与tail是在缓冲区数组内的下标值
    unsigned long tail;     // 
    struct task_struct* proc_list;  // 等待该当前tty设备的进程
    char buf[TTY_BUF_SIZE]; // 字义了一个缓冲区，它的大小是1K.
}


// 以下定义tty等待队列中缓冲区队列操作的一些宏函数, head指向可用空间的第一个位置，tail指向已空间的最后位置处
#define INC(a) ((a) = ((a) + 1) & (TTY_BUF_SIZE - 1))                           // increase， 在缓冲区内把指针循环向前移动一个字节
#define DEC(a) ((a) = ((a) - 1) & (TTY_BUF_SIZE - 1))                           // decrease
#define EMPTY(a) ((a).head == (a).tail)                                         // 判断一个tty队列是否为空
#define LEFT(a) (((a).tail - (a).head - 1) & (TTY_BUF_SIZE - 1))                //  判断一个缓冲队列内还有多少空闲字节
#define FULL(a) (!LEFT(a))                                                      // 判断缓冲队列是否满
#define CHARS(a) (((a).head - (a).tail) & *TTY_BUF_SIZE - 1)                    // 缓冲区中已经存放的字符个数
#define LAST(a) ((a).buf[(TTY_BUF_SIZE - 1) & ((a).head -1)])                   // 缓冲区队列内的最后一个字节，也就是最后放入的那个字节
#define GETCH(queue,c)   ({c = (queue).buf[(queue).tail]; INC((queue).tail);})  // 从缓存区队列的尾部获取一个字节
#define PUTCH(c, queue)  ({(queue).buf[(queue).head] = (c); INC((queue).head);})// 向缓存区队列的头部添加一个字节

/*  推算一下是否正确：
    这样写的队列，最多可以存放TTY_BUF_SIZE - 1个字节。
    假如一个缓冲区大小为0x10大字，即16字节。 初始化时,tail 与 head都指向了0x00处表示为空，然后向缓冲区内加入10个字节，此时
    tail指向了0x00处，head指向了0x0A处, 还有5个为空闲状态。 (0x00 - 0x0A - 1) & 0x0F = (-11) & 0x0f,
     -11在计算机内的表示为：100....00000000101, 该值与上0x0f,得0x05, 即5个空闲字节。
*/



// 获取终端键盘的字符类型, 从termios结构内的c_cc数组中拿
#define INTR_CHAR(tty) ((tty)->termios.c_cc[VINTR])         // 中断符
#define QUIT_CHAR(tty) ((tty)->termios.c_cc[VQUIT])         // 退出符
#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])       // 擦除符
#define KILL_CHAR(tty) ((tty)->termios.c_cc[VKILL])         // 终止符
#define EOF_CHAR(tty) ((tty)->termios.c_cc[VEOF])           // 文件结束符
#define START_CHAR(tty) ((tty)->termios.c_cc[VSTART])       // 开始符
#define STOP_CHAR(tty) ((tty)->termios.c_cc[VSTOP])         // 结束符
#define SUSPEND_CHAR(tty) ((tty)->termios.c_cc[VSUSPEND])   // 挂起符

// tty数据结构
struct tty_struct {
    struct termios termios;                 // 终端io属性和控制字符数据结构
    int pgrp;                               // 所属的进程组
    int stopped;                            // 停止标志
    void (*write)(struct tty_struct* tty)   // tty写函数指针
    struct tty_queue read_q;                // tty读队列
    struct tty_queue write_q;               // tty写队列
    struct tty_queue secondary;             // ttp用于存放规范模式字符的队列
};

extern struct tty_struct tty_table[];       // tty结构数组

/* 定义了数组c_cc的初始化值, 下面是值是使用八进制表示的，从前向后分别为：
   中断intr = ^C(\003), 退出quit = ^|(\034), 删除erase = del(\177), 终止kill = ^U(\025)
   文件结束eof = ^D(004), vtime = 0(\0), vmin = 1(\1), sxtc = 0(\0)
   开始start = ^Q(\021), 停止stop = ^S(\023), 挂起susp = ^Z(\032), 行结束eol=0(\0)
   重显reprint = ^R(\022), 丢弃discard = ^U(\017), werase=^W(\027), lnext = ^V(\026)
   行结束eol2=0(\0)
   */
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void  rs_init(void);        // 异步串行通信初始化
void con_init(void);        // 控制终端初始化
void tty_init(void);        // tty初始化

int tty_read(unsigned c, char* buf, int n);
int tty_write(unsigned c, char* buf, int n);

void rs_write(struct tty_struct* tty);
void con_write(struct tty_struct* tty);
void copy_to_cooked(struct tty_struct* tty);

#endif// #define _TTY_H
