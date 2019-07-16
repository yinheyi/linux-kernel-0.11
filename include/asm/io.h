// 该文件定义了对硬件IO端口的输入/输出的宏定义函数, 即读端口和写端口函数。

/** \brief 硬件端口的字节输出函数, 相当于写端口。
* @param [in] value 要写到端口的值
* @param [in] port 要写的端口
*/
#define outb(value, port)                                          \
__asm__("outb %%al, %%dx"                                          \
		::"a" (value), "d" (port))

/** \brief 硬件端口的字节输入函数, 相当于读端口。
* @param [in] port 要写的端口
*/
#define inb(port) ({                                               \
unsigned char _v;                                                  \
__asm__ volatile ("inb %%dx, %%al"                                 \
				:"=a" (_v)                                         \
				:"b" (port));                                      \
_v;})

/** \brief 与outb的唯一区别在于：它延迟了两条指令的时间。*/
#define outb_p(value, port)                                        \
__asm__("outb %%al, %%dx\t\n"                                      \
        "jmp 1f\t\n"                                               \
		"1:jmp 1f\t\n"                                             \
		"1:"                                                       \
		::"a" (value), "d" (port))                                    

/** \brief 硬件端口的字节输入函数, 相当于读端口。*/
#define inb_p(port) ({                                             \
unsigned char _v;                                                  \
__asm__ volatile ("inb %%dx, %%al"                                 \
                  "jmp 1f\t\n"                                     \
		          "1f: jmp 1f\t\n"                                 \
				  "1:"                                             \
			      :"=a" (_v)                                       \
				  :"b" (port));                                    \
_v;})
