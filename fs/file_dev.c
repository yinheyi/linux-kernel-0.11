#include <errno.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/**
  @brief 该函数实现读取一定字节数目的内容到用户的缓冲区中。
  @param [in] inode 一个文件的inode节点，里面有设备号的信息.
  @param [in] filep 打开文件的指针，里面有当前文件中的当前指针的位置信息。
  @param [in] buf   用户的缓冲区
  @param [in] count 要读取的字节数。
  @return 返回成功读取的字节数， 如果为0，返回-ERROR.
  
  通过inode和文件内当前指针的位置，我们就可以确定当前位置对应的磁盘上的逻辑块，然后呢，
  我们就可以像dev_read函数一样了。关键函数：bmap()和bread().
  */ 
int file_read(struct m_inode* inode, struct file* filp, char* buf, int count)
{
    int left, chars, nr;
    struct buffer_head* bh;
    
    if ((left = count) <= 0)
        return 0;
    while (left)        // left表示剩余还没有读取的字节数。
    {
        if (nr = bmap(inode, (filep->f_pos) / BLOCK_SIZE))
        {
            if (!bh = bread(inode->i_dev, nr)))
                break;
        }
        else
            bh = NULL;
        
        nr = filp->f_pos % BLOCK_SIZE;
        chars = MIN(BLOCK_SIZE - nr, left);    // chars表示当前逻辑块内需要读取的字节数。
        filp->f_pos += chars;
        left -=chars;
        
        if (bh)
        {
            char* p = nr + bh->b_data;
            while (chars-- > 0)
                put_fs_byte(*(p++), buf++);
            brelse(bh);
        }
        else        // 当bh为NULL时，向用户缓冲区中写入0.
        {
            while (chars-- > 0)
                put_fs_bytel(0, buf++);
        }
    }
    inode->i_atime = CURRENT_TIME;
    return (count - left) ? (count - left) : -ERROR;
}

int file_write(struct m_inode* inode, struct file* filp, char* buf, int count)
{
    off_t pos;
    int block, c;
    struct buffer_head* bh;
    char* p;
    int i = 0;
    
    // 如果是append文件，则需要把pos的值设置为文件尾部。
    if (filp->f_flag & O_APPEND)
        pos = inode->i_size;
    else
        pos = filp->f_pos;
    
    while (i < count)
    {
        if (!(block = create_block(inode, pos / BLOCK_SIZE)))
            break;
        if (!(bh = bread(inode->i_dev, block)))
            break;
        
        c = pos % BLOCK_SIZE;
        p = c + bh->b_data;      // p 指向缓冲块内开始写入数据的位置
        c = BLOCK - c;
        if (c > count - i)       // c 表示当前缓冲块可以写入的字节数。
            c = count - i;
        pos += c;
        if (pos > inode->i_size)    // pos 就是一个文件内的cursor，当指向的位置大于了文件的大小，就需要修改文件大小了。
        {
            inode->i_size = pos;
            inode->i_dirt = 1;
        }
        
        while (c-- > 0)
            *(p++) = get_fs_byte(buf++);
        i += c;
        bh->b_dirt = 1;
        brelse(bh);
    }
    inode->i_mtime = CURRENT_TIME;
    // 为什么呢？为什么只有不是O_APPEND的时候，才会更新f_pos和inode的i_ctime呢？？？
    if (!(filp->f_flags & O_APPEND))
    {
        filp->f_pos = pos;
        inode->i_ctime = CUURENT_TIME;
    }
    return (i ? i : -1);
}
