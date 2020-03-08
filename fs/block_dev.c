#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

/**
  @brief 该函数实现向指定的设备中写入一定字节长度的数据。
  @param [in]     dev   设备号
  @param [in,out] pos   这是一个指针，里面的值表示要写入点在硬盘中的字节偏移位置，都通它可以计算出对应的block块, 写入数据之后，该值会增加的.
  @param [in]     buf   用户存放数据的缓冲区位置(指针)
  @param [in]     count 要写入的字节数
  @return 返回值是成功写入的字节数。
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
        
        /*如果正好是一个逻辑块时，直接调用getblk函数在高速缓存内申请一个新的block块，往里写内容就可以。 如果不正好是
          一个数据块时，就需要使用bread函数把那一块都部读取到高速缓存中，然后再向里面写内容。 能是上一个
          使用者留下来的。 为了能够进行预读下两个块，所以这里使用了breada函数，该函数最后一个
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
    return written;
}

/**
  @brief 该函数实现从指定的设备中读出一定字节长度的数据。
  @param [in]     dev   设备号
  @param [in,out] pos   这是一个指针，里面的值表示要读取的位置在设备文件中的偏移位置, 读取数据之后，该值会增加的.
  @param [in]     buf   用户用于存放数据的缓冲区位置(指针)
  @param [in]     count 要读取的字节数
  @return 返回值是成功读取的字节数。
  */
int block_read(int dev, unsigned long* pos, char* buf, int count)
{
    int block = *pos >> BLOCK_SIZE_BITS;
    int offset = *pos & (BLOCK_SIZE - 1);
    int chars;
    int read = 0;
    struct buffer_head* bh;
    register char* p;

    while(count > 0)
    {
        chars = BLOCK_SIZE - offset;
        if (count < chars)
            chars = count;
        if (!(bh = breada(dev, block, block + 1, block + 2, -1)))    // 这是与写设备号不一样的：
            return read ? read : -EIO;
        p = offset + bh->b_data;
        while (char-- > 0)
            put_fs_byte(*(p++), buff++);
        brelse(bh);

        block++;
        offset = 0;
        *pos += chars;
        read += chars;
        count -= chars;
    }
    return read;
}
