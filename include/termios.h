#ifndef _TERMIOS_H
#define _TERMIOS_H


#define TTY_BUF_SIZE 1024

// tty调用命令
#define TCGETS  0x5401      // 取相应终端termios结构中的信息
#define TCSETS  0x5402      // 设置

#endif // #define _TERMIOS_H
