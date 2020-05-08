/**
  @file
  @brief 本程序包括字符设备的上层接口函数，主要含有终端读/写函数等。 
  */

#include < ctype.h>             // 字符类型头文件。
#include <errno.h>              // 错误号头文件
#include <signal.h>             // 信号头文件


// 下面给出了相应信号在信号位图中对应的比特位
#define ALRMMASK    (1 << (SIGALRM - 1))            // alarm
#define KILLMASK    (1 << (SIGKILL - 1))            // kill
#define INTMASK     (1 << (SIGINT - 1))             // interrupt, 键盘中断
#define QUITMASK    (1 << (SIGQUIT - 1))            // quit, 退出
#define TSTPMASK    (1 << (SIGTSTP - 1))            // tty发出的停止进程信号屏蔽码

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/segment.h>
#include <asm/system.h>

#define 
