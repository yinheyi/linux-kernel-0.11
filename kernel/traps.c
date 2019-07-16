/**

* /

#include <string.h>
#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

/** \brief 获取给定段给定偏移地址处的一个字节。
* @param [in] seg 段的选择子
* @param [in] addr 段内偏移地址
* @return 返回一个字节值
*/
#define get_seg_byte(seg, addr) ({                                      \
regeister char __res;                                                   \
__asm__("push %%fs\t\n"                                                 \
        "mov %%ax, %%fs\t\n"                                            \
        "movb %%fs:%2, %%al\t\n"                                        \
        "pop %%fs"                                                      \
        :"=a" (__res)                                                   \
        :"a" (seg), "m" (*(addr)));                                     \
__res;})

/** \brief 获取给定段给定偏移地址处的4个字节。
* @param [in] seg 段的选择子
* @param [in] addr 段内偏移地址
* @return 返回4个字节值
*/
#define get_seg_long(seg, addr) ({                                      \
regeister long __res;                                                   \
__asm__("push %%fs\t\n"                                                 \
        "mov %%ax, %%fs\t\n"                                            \
        "movl %%fs:%2, %%eax\t\n"                                       \
        "pop %%fs"                                                      \
        :"=a" (__res)                                                   \
        :"a" (seg), "m" (*(addr)));                                     \
__res;})

/** \brief 获取fs段选择子 *\
#define _fs() ({                                                        \
register unsigned short __res;                                          \
__asm__("mov %%fs, %%eax"                                               \
        :"=a" (__res):);                                                \
__res; })

