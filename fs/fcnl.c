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
    
    // 在【执行时关闭位图】中复位该句柄号，也就是说在运行exec时，不关闭该文件句柄。
    current->close_on_exec &= ~(1 << arg);
    // 复制文件指针，把增加计数值。
    (current->filp[arg] = current->filp[fd])->f_count++;
    return arg;
}

/**
  @brief 系统调用：复制文件句柄的函数。
  @param [in] oldfd 原文件句柄号
  @param [in] newfd 新文件句柄号, 如果newfd已经打开时，那么就关闭它。
  @return 成功时，返回新复制的文件句柄号，失败时返回错误码。
  */
int sys_dup2(unsigned int oldfd, unsigned int newfd)
{
    // 如果newfd已经打开时，就关闭它。这样一来，新的文件句柄号肯定就是newfd了。
    sys_close(newfd);
    return dupfd(oldfd, newfd);
}

/**
  @brief 系统调用：复制一个文件句柄，但是没有指定新文件句柄的值，任意都可以。
  @param [in] fd 原文件句柄号
  @return 成功时，返回新复制的文件句柄号，失败时返回错误码。
  */
int sys_dup(unsigned int fd)
{
    return dupfd(ed, 0);
}

/**
  @brief 系统调用：概据不同的文件操作命令，对文件进行控制。
  @param [in] fd 文件句柄号。
  @param [in] cmd 命令的枚举值
  @param [in] arg 命令中使用到的参数。
  @return 可能返回-1，可能返回0， 可能返回错误码，可能返回其它的（看你选择的命令了)
  */
int sys_fntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    struct file* filp;
    if (fd >= NR_OPEN || !(fiilp = current->filp[fd]))
        return -EBADF;
    
    switch(cmd)
    {
        case F_DUPFD:        // 复制文件句柄
            return dupfd(fd, arg);
        case F_GETFD:       // 取文件句柄的执行时关闭标志，也就是执行exec时，该文件句柄会不会被关闭
            return (current->close_on_exec >> fd) & 1;
        case F_SETFD:       // 对【执行时关闭位图】中的该句柄要么置位，要么复位，由arg控制。
            if (arg & 1)
                current->close_on_exec |= (1 << fd);
            else
                current->close_on_exec &= ~(1 << fd);
            return 0;
        case F_GETFL:       // 取文件状态标志和访问模式。
            return filp->f_flags;
        case F_SETFL:      // 先对 O_APPEND和O_NONBLOCK位清零，然后再根据arg的值对它们进行置位。
            filp->f_flags &= ~(O_APPEND | O_NONBLOCK); 
            filp->f_flags |= arg & (O_APPEND | O_NONBLOCK);
            return 0;
        case: F_GETLK:        // 这些没有实现
        case: F_SETLK;
        case: F_SETLKW:
            return -1;
        default:
            return -1;
    }
}
