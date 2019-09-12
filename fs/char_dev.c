#include <errno.h>
#include <sys/types.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/io.h>

extern int tty_read(unsigned minor, char* buf, int count);    // 终端读
extern int tty_write(unsigned minor, char* buf, int count);   // 终端写

typedef int (*crw_ptr)(int rw, unsigned minor, char* buf, int count, off_t* pos);

/**
  @brief  串口终端的读写函数。
  @param [in] rw 用户控制是读还是写
  @param [in] minor 终端的子设备号
  @param in] buf 用户的缓冲区
  @param [in] count 要读或要写的字节数
  @param [in] pos 当前位置的指针，对于终端来说，没有用。对应文件来说，应该是有用的！
  @return 
  */
static int rw_ttyx(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    if (rw == READ)
        return tty_read(minor, buf, count);
    else
        return tty_write(minor, buf, count);
}

/**
  @brief 终端读写函数。 
  
  该函数的功能与上面的类似，不一样的地方是：终端不一样，在rw_ttyx函数中，读取的子设备号
  是minor参数传入的，而该函数内要读取的子设备号是当前进拥有的子设备号，如果当前进程没有
  终端设备，则返回错误码。
  */
static int rw_tty(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    if (current->tty < 0)
        return -EPERM;
    return rw_ttyx(rw,  current->tty, buf, count, pos);
}

/**
  @brief 端口读写函数。
  @param [in] rw 控制进行读操作还是写操作
  @param [in] buf 缓冲区地址
  @param [in] count 要读取与写入的字节数
  @param [in] 端口地址
  @return  返回实际读/写的字节数。
  */
static int rw_port(int rw, char* buf, int count, off_t* pos)
{
    int i = *pos;
    
    while (count--> 0 && i < 65536)  // 端口号的地上不能超过64kb.
    {
        if (rw == READ)
            put_fs_byte(inb(i), buf++);
        else
            outb(get_fs_byte(buf++), i);
        ++i;
    }
    
    i -= *pos;        // 这个操作我有点傻啊？？这里在干吗？
    *pos += i;
    
    return i;
}

/**
  @brief 内存的读写函数.
  
  根据不同的子设备号，进行不同的读取操作。
  */
static int rw_meory(int rw, unsigned minor, char* buf, int count, off_t* pos)
{
    switch (minor)
    {
        case 0:
            return rw_ram(rw, buf, count, pos);
        case 1:
            return rw_mem(rw, buf, count, pos);
        case 2:
            return rw_kmem(rw, buf, count, pos);       // 内核
        case 3:
            return (rw == READ) ? 0 : count;           // 读写空, rw_null，无底洞。
        case 4: 
            return rw_port(rw, buf, count, pos);
        default:
            return -EIO;
    }
}

static crw_ptr crw_table[] =    // 主设备列表
{
    NULL,             // 空设备
    rw_memory,        // 内存相关
    NULL,             // 软驱
    NULL,             // 硬盘
    rw_ttyx,          // 串口终端
    rw_tty,           // 终端
    NULL,             // 打印机
    NULL
};
#define NRDEVS ((sizeof(crw_table)) / (sizeof(crw_ptr)))

/**
  @brief 字符设备读写操作函数。
  @param [in] rw 控制是读操作/写操作
  @param [in] dev 设备号，里面保存了主设备号和子设备号。
  @param [in] buf 用户缓冲区
  @param [in] count要读取/写入的字节数。
  @param [in] pos 选择不同的设备，它的作用不一样。
*/
int rw_char(int rw, int dev, char* buf, int count, off_t* pos)
{
    crw_ptr call_addr;
    if (MAJOR(dev) >= NRDEVS)
        return -ENODEV;
    
    if (!(call_addr = crw_table[MAJOR(dev)]))
        retrun -ENODEV;
    return call_addr(rw, MINOR(dev), buf, count, pos);
}
