#include <string.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <fcntl.h>
#include <sys/stat.h>

extern int sys_close(int fd);

/**
  @brief 该函数的功能是复制给定的文件句柄，并返回新复制之后的文件句柄号。
  @param [in] fd 原文件句柄号
  @param [in] arg 指定的新的文件句柄号的最小值。
  @return 成功时，返回新复制的文件句柄号，失败时返回错误码。
  
  什么是文件句柄号呢？？操作系统中，每一个打开的文件都对应一个file的数据结构，
  每一个进程都有一个数组用于存放该进程打开的文件的file数据结构的指针，该数组的项数
  为NR_OPEN, 一个文件的句柄号就是该文件指针在数组中的下标号。
  */
static int dupfd(unsigned int fd, unsigned int arg)
{
    // 当原文件句柄号超过了最大值或者它对应的文件指针不存在时，返回错误码。
    if (fd >= NR_OPEN || !current->filp[fd])
        return -EBADF;
    
    // 当新文件句柄号的最小值非法或找不到满足条件的新句柄号时，也返回错误码。
    if (arg >= NR_OPEN)
        return -EINVAL;
    while (arg < NR_OPEN)
    {
        if (current->filp[arg])
          ++arg;
        else
            break;
    }
    if (arg >= NR_OPEN)
        return -EMFILE;
    
    // 复制文件指针，把增加计数值。
    current->close_on_exec &= ~(1 << arg);
    (current->filp[arg] = current->filp[fd])->f_count++;
    return arg;
}

