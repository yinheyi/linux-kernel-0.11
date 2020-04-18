/**
  @file
  @brief 键盘驱动程序文件，它主要包括键盘中断处理程序。
  */

#include <linux/config.h>

.text
.globl_keyboard_interrupt
size = 1024         // 键盘缓冲区的大小，必须为2的指数，原因是在计算缓冲区空/满/可用大小/等时使用了按位运算。
head = 4            // 它表示在tty_queue结构体中head变量在偏移大小
tail = 8            // 它表示在tty_queue结构体中tail变量在偏移大小
proc_list = 12      // 它表示在tty_queue结构体中proc_list变量在偏移大小
buf = 16            // 它表示在tty_queue结构体中proc_list变量在偏移大小

/* 定义一个字节的变量用于表示键盘特殊盘按下的状态标志. 一个字节共有8位，表示的标志分别是：
   7位：caps 键按下     6位：caps键的状态               5位：右alt键按下    4位：左alt键按下
   3位：右ctrl键按下    2位：左ctrl键按下               1位：右shift键按下  0位：左shift键按下

   解释一下caps键状态：每当我们按一次caps键，它的状态就会反转一次，假如之前状态为0， 按一次caps键，它的
   状态就会变成1(这时候输入是大写模式了),再按一次它的状态又加成了0(这时候输入是小写模式了)
  */
mode:   .byte 0

/* 定义了键盘上3个LED灯的状态，只使用到了字节的低3位:
   2位：caps_lock键对应的led灯      1位：num-lock键对应的led灯      0位：scroll-lock键对应的led灯 */
leds:   .byte 2

/* 当扫描码是0xe0或0xe1时置该标志, 只使用到了该字节的低2位：
   1位：当收到0xe1时置位        0位：当收到0xe0时置位   */
e0:     .byte 0 

# 键盘中断处理程序入口点

/* 键盘中断处理子程序： */
_key_board_interrupt:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    movl $0x10, %eax        // 设置ds与es段寄存器
    mov %ax, %ds
    mov %ax, %es
    xorl %al, %al           # 使用按位异或指令，把al寄存器清零操作
    inb $0x60, %al          # 读取扫描码到AL中
    cmpb $0xe0, %al         # 判断扫描码是否为0xe0,如果是，则跳转, 因为收到它们需要设置e0内的相应标志位, 并且不需要调用key_table内的函数。
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


/* 下面的子程序实现把eax和ebx中的字符加入到缓冲区队列中,在添加过程中, 每一次都是添加al的值，然后把其它的值位移到al中。
   在添加过程中，如果eax为0，就停止加入了。 另外，添加时，要么全部加入，要么一个也不加入,因为如果缓冲区满时，不会更新
   tty_quque结构体的在内存的head的值。
  */
put_queue:
    pushl %ecx
    pushl %edx
    movl _table_list %edx       // _table_list存放的是读写队列(tty_queue)的地址
    movl head(%edx), %ecx       // 这里是间接寻址，相当于取(%edx+head)地址处的值放到%ecx中,即 %ecx = *(%edx + head)
                                // 此时，ecx中保存的是tty_quque中head变量的值，它表示缓冲区head在缓冲区的偏移位置.
                                // 当我们向缓冲区内存放数据时，是向head处存放(head指向了第一个空位置)；当我们从缓冲区
                                // 读数据时从tail读取(tail指向了最后一个非空的位置)
1:  movb %al, buf(%edx, %ecx)   // edx存放的是tty_queue结构体的地址,buf是存放的是buf[1024]数组在tty_queue中偏移大小，
                                // ecx存放的是head在buf[1024]中的偏移大小，因此，该汇编语言实现：*(edx+ecx*1+buf) = al,
                                // 即把al中的字符放到缓冲队列头位置处。
    incl %ecx;                  // 下面两句汇编代码实现ecx内的值循环加1(意思就是加1之后如果超过了缓冲区的大小，就为0)
    andl $size-1, %ecx
    cmpl tail(%edx), %ecx       // 这是判断tail是否与ecx(它表示了head+1的值, 因为上面两行代码只是把寄存器加1了，但是没有同步
                                // 到内存中)相同，如果相同，说明了tail = head + 1， 这就表示了缓冲区已经满了，上面添加到缓冲区
                                // 中al值其实也作废了(因为没有更新head的值),然后直接跳到3f处退出了。
    je 3f
    shrdl $8, %ebx, %eax        // shrdl指令很有意思，首先eax向右移动8位，此时al就被覆盖了，eax空出来的高8位由ebx的低8位进行填充，
                                // 但是ebx内的值保持不变。当操作完成时，如果eax为零，则会置ZF位
    je 2f                       // 这是判断eax是否为0，如果为0时，就停止写入缓冲区了，跳转处理
    shrl $8, %ebx               // 对ebx寄存器向右位移8字节, 因为上面已经把ebx中的低8位放到了eax中的高8位中了
    jmp 1b                      // 跳转，继续执行写缓冲区操作。
2:  movl %ecx, head(%edx)       // 把寄存器的表示的head值同步更新到内存中, 这些才相当于真正把数据写入到了缓冲区中
    movl proc_list(%edx), %ecx  // 把等待该缓冲区队列的进程指针值放到了ecx中。
    testl %ecx, %ecx            // 测试ecx的值是否为空(testl， 对两个操作数执行and操作，但是只修改标志位，不会修改操作数的值)
    je 3f                       // 如果为空，就跳转退出, 不为空时，就进程的state(它在task_struct结构体中的偏移为0)位置为0，表示可运行状态
    movl $0, (ecx)
3: popl %edx
   popl %ecx
   ret

/* 该子程序实现：当按下ctrl或atl时设置mode字节中的对应的位为1. 左ctrl对应了mode第2位(00000100), 左alt对应了
   mode中的第4位(00010000), 右ctrl对应了mode中的第3位(00001000), 右alt对应了mode中的第5位(00100000).
   如果之前收到过0xe0扫描码, 则说明按下的是键盘中右边的ctrl或alt键。 意思就是说：无论左边还是右边的ctrl或alt,
   它们对应的扫描码是相同的，但是如果之前收到过0xe0扫描码，则认为它是右边的ctrl或alt键。 */
ctrl: 
    movb $0x04, %al     // ctrl对应了第2位为1: 00000100
    jmp 1f
atl:
    movb $0x10, %al     // alt对应了第4位为1：00010000
1:  cmpb $0, e0         // 看看e0字节中的数据是否为0，如果为0，说明没有收到exe0扫描码，如果不为空,说明可能收到了exe0，也可能收到了
    je2f                // 0xe1扫描码啊, 这是不太明白了。
    addb %al, %al       // 如果之前收到了0xe0了，则al+al之后: 00000100会变成00001000, 00010000会变成00100000.
2:  orb %al, mode       // 置mode中相应的位为1.
    ret


/* 该子程序处理的情况与上面正好相反：当松开ctrl或alt键时，设置mode字节中的相应位为0. 代码与上面的子程序基本相同 */
unctrl:
    movb $0x04, %al
    jmp 1f
unalt:
    movb $0x10, %al
1:  cmpb $0, e0
    je2f
    addb %al, %al
2:  notb %al
    andb %al, mode
    ret

/** 下面子程序分别实现：收到和公开左右shift键时，设置或复位mode中的相应位。 */
lshift:
    orb $0x01, mode
    ret
unlshift:
    add $0xfe, mode
    ret
rshift:
    orb $0x02, mode
    ret
unrshift:
    andb $0xfd, mode
    ret

caps:
    testb $0x80, mode
    jne 1f
    xorb $4, leds           // 通过同或操作，
    xorb $0x40, mode
