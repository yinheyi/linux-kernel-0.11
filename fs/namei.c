#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

/**
  @brief 
  @details \004、\002、\006、\377是4个8进制的转义字符，该宏就是根据x值取4个转义字符中的一个。
  */
#define ACC_MODE(x) ("\004\002\006\377"[(x)&_ACCMODE])

#define MAY_EXEC 1
#define MAY_WRITE 2
#defin MAY_READ 4
