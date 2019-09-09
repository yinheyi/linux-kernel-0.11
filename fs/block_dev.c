#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

/**
  @brief 该函数实现向指定的设备中写入一定字节长度的数据。
  @param
  @return
  */
int block_write(int dev, long* pos, char* buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;
    int offset = *pos & (BLOCK_SIZE - 1);
    int chars;
    int written = 0;
    struct buffer_head* bh;
    register char* p;
    
    while (count > 0)
    {
        chars = BLOCK_SIZE - offset;        // 当前逻辑块可以存放的字节数
        if (count < chars)
            chars = count;
        
        /*如果正好是一个逻辑块时，它调用了getblk函数，而没有调用bread函数，因为此时是写逻辑块，不需要
          进行高速缓冲块和磁盘上数据的同步。
          如果不正好是一个数据块时，就需要使用bread函数进行数据块的同步,因为当前的高速缓冲块内的数据可
          能是上一个使用者留下来的。 为了能够进行预读下两个块，所以这里使用了breada函数，该函数最后一个
          参数需要是负数，表示参数列表的结束。  */
        if (chars == BLOCK_SIZE)
            bh = getblk(dev, block);
        else
            bh = breada(dev, block, block + 1, block + 2, -1);
        if (!bh)
            return written ? written : -EIO;
        
        p = offset + bh->b_data;
        while (chars-- > 0)
            *(p++) = get_fs_byte(buf++);
        bh->b_dirt = 1;
        brelse(bh);
        
        block++;
        offset = 0;
        *pos += chars;
        written += chars;
        count  -= chars;
    }
}
