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

/**
  @brief 定义了一个内嵌汇编谎言的宏函数，实现的功能是：对一个long类型变量的指定位设置为0，并且返回原来的值(1或0).
  @param [in] nr 指定第几位。
  @param [in] addr 要设计的变量的地址。
  @return 返回0或1, 代表了你要设置的那个位原来的值。
  
  相关的汇编指令说明：  
  - btrl src, dest  把src索引号指定的dest上的目的位复制到CF标志位上，并把目的位置为0.
  - setb src  src为一个字节，如果CF位为1，则把src设置为1，如果CF位为0，则把src设置为0.
  */
#define clear_bit(nr, addr) ({                                      \
register int res __asm__("ax");                                     \
__asm__ __volatile__("btrl %2, %3\n\t"                              \
                     "setb %%al"                                    \
                     :"=a"(res)                                     \
                     :"0"(0), "r" (nr), "m" (*(addr)));             \
res;})

/**
  @brief 该宏函数实现了从给定地址处查找第一个为0的位，返回查找到的下标，如果没有查找到，则返回0.
  该宏函数只查找从addr处偏移到1kb的内存空间。
  @param [in] addr 给定的地址。
  @return 返回第一个为0的位的下标。
  
  涉及到的汇编指令包含：  
  - cld 指令：清空方向位DF， 使得一些字符串指令增加si和di的值。
  - lodsl指令：load string，把ds:si地址处的一个long类型变量放到eax寄存器中。
  - notl src 指令：对每一位进行逻辑
  - bsfl src des 指令： bit scan forward, 从src中查找（由0位至n位)查找第一个为1的位。如果
    查找到，则把ZF置1，并且把相应位的下标索引值放到des中；如果查找不到，ZF位则置为0.
  */
#define find_first_zero(addr) ({                                   \
int __res;                                                         \
__asm__("cld\n\t"                                                  \
        "1: lodsl \n\t"                                            \
        "notl %%eax\n\t"                                           \
        "bsfl %%eax, %%edx\n\t"                                    \
        "je 2f\n\t"                                                \
        "addl %%edx, %%ecx\n\t"                                    \
        "jmp 3f\n"                                                 \
        "2: addl $32, %%ecx\n\t"                                   \
        "cmpl $8192, %%ecx\n\t"                                    \
        "jl 1b\n"                                                  \
        "3:"                                                       \
        :"=c"(__res)                                               \
        :"c"(0), "S"(addr)                                         \
        :"ax", "dx", "si");                                        \
__res;})
