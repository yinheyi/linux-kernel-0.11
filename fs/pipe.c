#include <signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/segment.h>

/*
   1. 管道就是一个内存缓冲区，它的大小是一个页！管道的读与写是典型的生产者-消费者模式. 读写管道的
   代码我个人认为写的非常好，非常值得学习。
   2. 管道尾指针指向了管道内数据的最后位置，它指向的位置是已经存放数据！
   3. 管道的头指针指向了管道内待存放数据的下一个空间位置！
   4. 当管道尾指针与管道头指针重合时，说明管道内的数据为空。
   5. 当管道尾指针减去管道头指针的大小为全部缓冲区 - 1 时， 说明此时的管道空间已经是满的.
   6. 管道内数据的多少 直接使用(头指针 - 尾指针 ) & (PAGE - 1)就可以得到！
 */


/*
   定义在fs.h头文件中的宏，本文件中的代码会使用到!
#define PIPE_HEAD(inode) ((inode).i_zone[0])      // 相对于缓冲区起始处的偏移位置
#define PIPE_TAIL(inode) ((inode).i_zone[1])      
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)) - PIPE_TAIL(inode)) & (PAGE_SIZE - 1)
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode) == PIPE_TAIL(indoe))
#define PIPE_FULL(inode) (PIPE_SIZE(inode) == (PAGE_SIZE - 1))
 */


/**
  @brief 该函数负责读取管道中的字符.在读取过程中，可能读着读着没有数据可读了，此时唤醒可能
         存在的要写该管道的进程，然后自己休眠，等待写进程写满管道时，唤醒当前进程写继续读！
  @param [in] inode 管道对应的i节点
  @param [in] buf   缓冲区地址
  @param [in] count 要读取的字字数
  @return 返回值是成功读取的字节数
  */
int read_pipe(struct m_inode* inode, char* buf, int count)
{
    int chars, size, read = 0;
    while (count > 0)
    {
        /* 当管道内的字符为空时，看看是否有写该管道的进程，如果存在, 就唤醒那个进程，让它
           向管道写入数据供你读，如果不存在的话，那只好返回已经读取的字节数了! */
        while (!(size = PIPE_SIZE(*inode)))
        {
            wake_up(&inode->i_wait);
            if (inode->i_count != 2)
                return read;
            sleep_on(&inode->i_wait);
        }

        chars = PAGE_SIZE - PIPE_TAIL(*inode);    // 读取缓冲区边界处(不调头之前)，可以读取的字节数
        if (chars > count)
            chars = count;
        if (chars > size)
            chars = size;

        read += chars;
        count -= chars;
        size = PIPE_TAIL(*inode);               // 可以看出来，PIPE_TAIL和PIPE_HEAD是相对的缓冲区的偏移位置！
        PIPE_TAIL(*inode) += chars;
        PIPE_TAIL(*inode) &= (PAGE_SIZE - 1);   // 该语句很重要，它可能处理了调头的过程！

        while (char-- > 0)
            put_fs_byte(((char*)inode->i_size)[size++], buf++);
    }
    wake_up(&inode->i_wait);                   // 最后一定要记得唤醒等待该管道的进程！
    return read;
}

/**
  @brief 该函数负责向管道内写入字符.在写的过程中，可能写满了，没有空间了，此时唤醒可能
         存在的要读该管道的进程，然后自己休眠，等待读进程读空了管道时，唤醒当前进程再继续写！
  @param [in] inode 管道对应的i节点
  @param [in] buf   缓冲区地址
  @param [in] count 要写入的字节数
  @return 返回值是成功写入的字节数
  */
int write_pipe(struct m_inode* inode, char* buf, int count)
{
    int chars, size, written = 0;
    while (count > 0)
    {
        if (!(size = (PAGE_SIZE - 1 - PIPE_SIZE(*inode))))
        {
            wake_up(&inode->i_wait);
            if (inode ->i_count != 2)
            {
                current->signal |= (1 << (SIGPIPE - 1));
                return written ? written : - 1;
            }
            sleep_on(&inode->i_wait);
        }

        chars = PAGE_SIZE - PIPE_HEAD(*inode);
        if (chars > count)
            chars = count;
        if (chars > size)
            chars = size;

        count -= chars;
        written += chars;
        size = PIPE_HEAD(*inode);
        PIPE_HEAD(*inode) += chars;
        PIPE_HEAD(*inode) &= (PAGE_SIZE - 1);

        while (chars-- > 0)
            ((char*)inode->i_size)[size++] = get_fs_byte(buf++);
    }
    wake_up(&inode->i_wait);
    return written;
}

/**
  @brief 系统调用：创建一个无名的管道； 一个inode上绑定了两个文件指针，一个用于读，一个用于写，并返回对应的文件描述符号。
  @param [out] fildes 它是一个数组指针，用于存放文件描述符号。
  @return 成功时返回0， 出错是返回-1.
  */
int sys_pipe(unsigned long* fildes)
{
    struct m_inode* inode;
    struct file* f[2];
    int fd[2];
    int i, j;
    
    // 从系统的文件列表中查找空间的文件项，并把相应的指针放到f[]数组内。
    for (i = 0, j = 0; i < NR_FILE && j < 2; i++)
    {
        if (!file_table[i].f_count)
        {
            file_table[i].f_count++;
            f[j++]= file_table + i;
        }
    }
    if (j == 1)
        f[0]->f_count = 0;
    if (j < 2)
        return -1;
    
    // 从当前进行的文件数组中查找空闲位置，并把上面找到的文件指针放进去，还把对应的文件描述符放到fd[]数组中。
    for (i = 0; j = 0; j < NR_OPEN && j < 2; ++i)
    {
        if (!current->filp[i])
        {
            fd[j] = i;
            current->filp[i++] = f[j++];
        }
    }
    if (j == 1)
        current->filp[fd[0]] = NULL;
    if (j < 2)
    {
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    
    // 新建一个inode, 给管道使用。
    if (!(inode = get_pipe_inode()))
    {
        current->filp[fd[0]] = current->filp[fd[1]] = NULL;
        f[0]->f_count = f[1]->f_count = 0;
        return -1;
    }
    f[0]->f_inode = f[1]->f_inode = inode;
    f[0]->f_pos = f[1]->f_pos = 0;
    f[0]->f_mode = 1;        // 读
    f[0]->f_mode = 2;         // 写
    put_fs_long(fd[0], fildes);
    put_fs_long(fd[1], fildes + 1);
    return 0;
}
