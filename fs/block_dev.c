#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

/**
  @brief
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
        
    }
}
