/**
  @file fcntl.h 
  @brief 文件控制选项头文件, 主要定义了函数fcntl()和open()中使用的到一些选项和符号。
  */

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

// 文件访问模式, 注意下面的定义说明：
// 它没有使用0x01表示是否可读，0x02表示是否可写，0x03表示可读可写, 而是使用
// 0x00表示只读，0x01表示只写，0x02表示可读可写。 原因应该是： 把只读/只写/可读可写
// 是三种独立的状态，而不是说是否具有读/写权限(此时可以使用一个位表示可读位,一个位表示可写位)。
#define O_ACCMODE         0003
#define O_RDONLY          0000
#define O_WRONLY          0001
#define O_RDWR            0002

// 文件的打开标志位, 用于控制文件打开时的动作, 用于函数open()中。
#define O_CREAT          00100        // 文件不存在时，就创建
#define O_EXCL           00200        // 独占使用文件标记
#define O_NOCTTY         00400        // 不分配控制终端
#define O_TRUNC          01000        // 若文件已经存在且是写操作，则长度截为0
#define O_APPEND         02000        // 以添加的方式打开文件
#define O_NONBLOCK       04000        // 非阻塞文件打开和操作文件
#define O_NDELAY         O_NONBLOCK 

#endif // _FCNTL_H
