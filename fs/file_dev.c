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
  @param [in] filep 打开文件的指针，里面有当前文件当前指针的位置信息。
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
}
