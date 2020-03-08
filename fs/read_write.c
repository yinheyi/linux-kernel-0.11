#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/segment.h>

extern int rw_char(int rw, int dev, char* buf, int count, off_t* pos);
extern int read_pipe(struct m_inode* inode, char* buf, int count);
extern int write_pipe(struct m_inode* inode, char* buf, int count);
extern int block_read(int dev, off_t* pos, char* buf, int count);
extern int block_write(int dev, off_t* pos, char* buf, int count);
extern int file_read(struct m_indoe* inode, struct file* filp, char* buf, int count);
extern int file_write(struct m_indoe* inode, struct file* filp, char* buf, int count);

/**
  @brief 重定位文件的读写指针的函数, 在文件struct内有有一个f_pos的变量，该函数就是对它进行修改。
  @param [in] fd 文件描述符， 给定了文件描述符，就可以从当前进程中找到文件指针了，文件struct内有文件对应的pos和inode信息。
  @param [in] offset 新定位时的偏移量, 单位就是字节。
  @param [in] origin 新定位时偏移量的相对位置，当origin = 0时，表示从文件开头设置偏移量, 当origin = 1时，表示从当前的读写
              位置设置偏移量，当origin = 2时，表示从文件尾设置偏移量。
  @return 返回文件新的读写位置。
  */
int sys_lseek(unsigned int fd, off_t offset, int origin)
{
    struct file* file;

    // 如果文件描述符大于最大值，或者 文件描述符对应的当前进程的文件指针为空 或者 文件没有对应的inode 或者 inode对应的设备不可以seek
    // 可以seek的设备为： 内存/软驱/硬盘, 对应的主设备号为1,2和3.
    if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(file->f_inode) || !IS_SEEKABLE(MAJOR(file->f_inode->dev)))
        return -EBADF;

    // 管道也不支持
    if (file->f_inode->i_pipe)
        return -ESPIPE;

    switch(origin)
    {
        case 0:
            if (offset < 0)
                return -EINVAL;
            file->f_pos = offset;
            break;

        case 1:
            if (file->f_pos + offset < 0)
                return -EINVAL;
            file->f_pos += offset;
            break;
        case 2:
            if (file->f_inode->i_size + offset < 0)
                return -EINVAL;
            file->f_pos = file->f_inode->i_size + offset;
        default:
            return -EINVAL;
    }
    return file->f_pos;
}

/**
  @brief 系统调用：读函数, 具体可以分为：读管道文件/读字符文件/读块设备文件/读常规文件.
  @param [in] fd 文件句柄，也就是文件描述符吧，它表示对应的文件指针在当前进程中的文件指针数组内的下标值。
  @param [out] buf 缓冲区的位置
  @param [in] count 要读取的字节大小

  @return  成功时返回读到字节数，失败时返回错误码。
  */
int sys_read(unsigned in fd, char* buf, int count)
{
    struct file* file;
    struct m_inode* inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
        return -EINVAL;

    if (count == 0)
        return 0;

    verify_area(buf, count);
    inode = file->f_inode;

    // 管道文件
    if (inode->i_pipe)
        return (file->f_mode & 1) ? read_pipe(inode, buf, count) : -EIO;

    // 字符文件
    if (S_ISCHR(inode->i_mode))
        return rw_char(READ, inode->i_zone[0], buf, count, &file->f_pos);

    // 块设备文件
    if (S_ISBLK(inode->i_mode))
        return block_read(inode->i_zone[0], &file->f_pos, buf, count);

    // 常规文件
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode))
    {
        if (count + file->f_pos > inode->i_size)
            count = inode->i_size - file->f_pos;
        if (count <= 0)
            return 0;
        return file_read(inode, file, buf, count);
    }

    printk("(Read)inode->i_mode = %06o\n\r", inode->i_mode);
    return -EINVAL;
}

/**
  @brief 
  */
int sys_write(unsigned int fd, char* buf, int count)
{
    struct file* file;
    struct m_inode* inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
        return -EINVAL;

    if (count == 0)
        return 0;

    inode = file->f_inode;
    if (inode->i_pipe)
        return (file->f_mode & 2) write_pipe(inode, buf, count) : -EIO;

    // 字符文件
    if (S_ISCHR(inode->i_mode))
        return rw_char(WRITE, inode->i_zone[0], buf, count, &file->f_pos);

    // 块设备文件
    if (S_ISBLK(inode->i_mode))
        return block_write(inode->i_zone[0], &file->f_pos, buf, count);

    // 常规文件
    if (S_ISREG(inode->i_mode))
        return file_write(inode, file, buf, count);

    printk("(Write)inode->i_mode = %06o\n\r", inode->i_mode);
    return -EINVAL;
}
