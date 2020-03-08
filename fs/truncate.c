#include <linux/sched.h>
#include <sys/stat.h>

/**
  @brief 释放给定设备的指定块上的一次间接块缓冲区,  并且存放一次间接块的block块也会被释放掉的。
  @param [in] dev 给定的设备号
  @param [in] block 设备上的指定块, 上面保存了一次间接块的块号。
  @return 返回空。
  */
static void free_ind(int dev, int block)
{
    struct buffer_head* bh;
    unsigned short* p;
    int i;

    if (!block)
        return;

    if (bh = bread(dev, block))
    {
        p = (unsigned short*)bh->data;
        for (i = 0; i < 512; ++i, ++p)    // 每一个逻辑块上有512个块号
        {
            if (*p)
                free_block(dev, *p);
        }
        brelse(bh);
    }
    free_block(dev, block);
}

/**
  @brief 释放给定设备的指定块上的二次间接块缓冲区,  并且存放二次间接块的block块也会被释放掉的。
  @param [in] dev 给定的设备号
  @param [in] block 设备上的指定块, 上面保存了二次间接块的块号。
  @return 返回空。
  */
static void free_dind(int dev, int block)
{
    struct buffer_head* bh;
    unsigned short* p;
    int i;

    if (!block)
        return;

    if (bh = bread(dev, block))
    {
        p = (unsigned short*)bh->b_data;
        for (i = 0; i < 512; ++i, ++p)
        {
            if (*p)
                free_ind(dev, *p);
        }
        brelse(bh);
    }
    free_block(dev, block);
}

/**
  @brief 释放给定inode结点占用的所有逻辑块号，并且把文件大小置为0.
  @param [in] inode 给定的inode结点指针。
  @return 返回值为空。
  */
void truncate(struct m_inode* inode)
{
    int i;

    // 不是目录或者常规文件，就返回。
    if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
        return;

    // 释放7个直接块
    for (i = 0; i < 7; ++i)
    {
        if (inode->i_zone[i])
        {
            free_block(inode->i_dev, inode->i_zone[i]);
            inode->i_zone[i] = 0;
        }
    }

    // 释放间接块
    free_ind(inode->i_dev, inode->i_zone[7]);
    free_dind(inode->i_dev, inode->i_zone[8]);
    inode->i_zone[7] = inode->i_zone[8] = 0;

    inode->i_size = 0;
    inode->i_dirt = 1;
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}
