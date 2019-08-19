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
