#include <string.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/**
  @brief 定义了一个内嵌汇编语言的宏函数， 作用是把给定地址的block大小的内存清空为零。
  @param [in] addr 给定的地址

  stosl汇编指令的作用是： store string, 把ax中的值存储到es:di处的内存空间中，并更新di的值。
  */
#define clear_block(addr)                                           \
    _asm__("cld\n\t"                                                \
            "rep\n\t"                                               \
            "stosl"                                                 \
            ::"a" (0), "c" (BLOCK_SIZE / 4), "D" ((long)(addr))     \
            : "cx", "di")

/**
  @brief 定义了一个内嵌汇编谎言的宏函数，实现的功能是：对一个long类型变量的指定位设置为1，并且返回原来的值(1或0).
  @param [in] nr 指定第几位。
  @param [in] addr 要设计的变量的地址。
  @return 返回0或1, 代表了你要设置的那个位原来的值。
  
  相关的汇编指令说明：  
  - btsl src, dest  把src索引号指定的dest上的目的位复制到CF标志位上，并把目的位置为1.
  - setb src  src为一个字节，如果CF位为1，则把src设置为1，如果CF位为0，则把src设置为0.
  */
#define set_bit(nr, addr) ({                                        \
register int res __asm__("ax");                                     \
__asm__ __volatile__("btsl %2, %3\n\t"                              \
                     "setb %%al"                                    \
                     :"=a"(res)                                     \
                     :"0"(0), "r" (nr), "m" (*(addr)));             \
res;})

