/**
  @file
  @brief 键盘驱动程序文件，它主要包括键盘中断处理程序。
  */

#include <linux/config.h>

.text
.globl_keyboard_interrupt
size = 1024
head = 4
tail = 8
proc_list = 12
buf = 16

/* 定义一个字节的变量用于表示键盘特殊盘按下的状态标志. 一个字节共有8位，表示的标志分别是：
   7位：caps 键按下     6位：caps键的状态(不太明白)     5位：右alt键按下    4位：左alt键按下
   3位：右ctrl键按下    2位：左ctrl键按下               1位：右shift键按下  0位：左shift键按下  */
mode:   .byte 0

/* 定义了键盘上3个LED灯的状态，只使用到了字节的低3位:
   2位：caps_lock键对应的led灯      1位：num-lock键对应的led灯      0位：scroll-lock键对应的led灯 */
leds:   .byte 2

/* 当扫描码是0xe0或0xe1时置该标志, 只使用到了该字节的低2位：
   1位：当收到0xe1时置位        0位：当收到0xe0时置位   */
e0:     .byte 0 

# 键盘中断处理程序入口点
_key_board_interrupt:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    xorl %al, %al           # 使用按位异或指令，把al寄存器清零操作
    inb $0x60, %al          # 读取扫描码到AL中
    cmpb $0xe0, %al         # 判断扫描码是否为0xe0,如果是，则跳转
    je set_e0
    cmpb $0xe1, %al         # 判断扫描码是否为0xe1,如果是，则跳转
    je set_e1
    call key_table(, %eax, 4)  # 调用key_table + eax * 4 处的函数
    movb $0, e0             # 复位e0标志
e0_e1:
    # 50行到60行用于对PC键盘电路进行复位处理
    inb $0x61, %al      // 从端口0x61中读取数据到AL寄存器
    jmp 1f              // 延时一下
1:  jmp 1f
1:  orb $0x80, %al      // 把读取到的数据的第7位置1(orb，或操作)
    outb %al, $0x61     // 把修改后的数据对回到了0x61端口内，到这里就禁止了键盘
    jmp 1f              // 再延时一下
1:  jmp 1f
1:  addb $0x7F, %al     // 通过与操作, 把第7位置0
    outb %al, $0x61     // 又把处理后的数据写回到0x61端口，这样就又许了键盘
    movb $0x20, %al
    outb %al, $0x20     // 向8259A中断芯片发送EOI(中断结束)信号。
    pushl $0
    call _do_tty_interrupt  // 调用do_tty_interrupt函数
    addl $4, %esp
    pop %es
    pop %ds
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    iret
set_e0:
    movb $1, e0         // 设置e0字节中第0位为1， 表示收到了0xe0字符
    jmp e0_e1
set_e1:
    movb $2, e0         // 设置e0字节中第1位为1， 表示收到了0xe1字符
    jmp e0_e1


/*
 */
put_queue:
    pushl %ecx
    pushl %edx
    movl _table_list %edx

