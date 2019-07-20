/** @file 该文件定义了系统调用的处理函数、时钟中断的处理函数以及硬盘和软盘中断的处理函数。
*/

SIG_CHID = 17

/* 各种寄存器的值在栈中偏移量(offset(%esp)) */
EAX = 0x00
EBX = 0x04
ECX = 0x80
EDX = 0xC0
FS = 0x10
ES = 0x14
DS = 0x18
EIP = 0x1c
CS = 0x20
EFLAGS = 0x24
OLDESP = 0x28
OLDSS = 0x2C

/* 任务结构体中一些变量的偏移量 */
state = 0
counter = 4
priority = 8
signal = 12
sigaaction = 16
blocked = 33 * 16

/* signaction 结构体中一些变量的偏移量 */
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

/* 总的系统调用数目 */
nr_system_calls = 72

.globl _system_call, _sys_fork, _timer_interrupt, _sys_execve
.globl _hd_interrupt, _floppy_interrupt, _parallel_interrupt
.globl _device_not_available, _coprocessor_error

.align 2
bad_sys_call:            
    movl $-1, %eax
    iret
.align 2
reschedule:
    pushl $ret_from_sys_call        # 把中断返回之后的执行位置的地址放入栈的目的是为了_schedule执行完之后，继续执行ret_from_sys_call.
    jmp _schedule
.align 2
_system_call:
    cmpl $nr_system_calls - 1, %eax
    ja bad_sys_call              # ja指令： 当CF位和ZF都清零时，会执行跳转
    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx
    pushl %ebx
    movl $0x10, %edx
    mov %dx, %ds
    mov %dx, %es
    movl $0x17, %edx
    mov %dx, %fs

    call _sys_call_table(, %eax, 4)    # 这里使用到了at&t汇编指令的寻址方式
    pushl %eax                         # eax寄存器存放了系统调用的返回地址。
    
    movl _current， %eax
    cmpl $0, state(%eax)
    jne reschedule
    cmpl $0, counter(%eax)
    je reschedule
    
res_from_sys_call:
    movl _current, %eax
    cmpl _task, %eax            # 判断当前任务是否为0任务
    je 3f
    cmpw $0x0f, CS(%esp)
    jne 3f
    cmpw $0x17, OLDSS(%esp)
    jne 3f
    
    movl signal(%eax), %ebx
    movl blocked(%eax), %ecx
    notl %ecx
    andl %ebx, %ecx
    bsfl %ecx, %ecx            # bsf指令：从左右查找第一个为1的位，如果找到，则把偏移值(0~31)放到des寄存器中，如果找不到，则置ZF位为1.
    je 3f
    btrl %ecx, %ebx            # btr指令：bit test and reset, 把指定位置的清零，并把清零之前的值复制到CF标志位中。
    mov %ebx, signal(%eax)
    
    incl %ecx                  # 因为do_signal函数接收的信号量的值为1-32之间，这里加1之后，就会调用do_signal函数。
    pushl %ecx
    call _do_signal
    
    popl %eax
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    pop %fs
    pop %es
    pop %ds
    iret
    
.align 2
_coprocessor_error:
    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    movl $0x17, %eax
    mov %ax, %fs
    pushl $ret_from_sys_call
    jmp _math_error
    
.align 2
_device_not_available:
    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    movl $0x17, %eax
    mov %ax, %fs
    pushl $ret_from_sys_call
    clts                   # clear task switch flag
    movl %cr0, %eax
    testl $0x4, %eax       # 测试EM位
    je _math_state_restore
    pushl %ebp
    pushl %esi
    pushl edi
    call _math_emulate
    popl %edi
    popl %esi
    popl %ebp
    ret
    
.align 2
_timer_interrupt:
    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    movl $0x17, %eax
    mov %ax, %fs
    incl _jiffies
    
    movb $0x20, %al
    outb %al, $0x20
    
    movl CS(%esp), %eax
    addl $3, %eax
    pushl %eax
    call _do_timer
    addl $4, %esp
    jmp ret_from_sys_call
    
.align 2
_sys_execve:
    lea EIP(%esp), %eax
    pushl %eax
    call _do_execve
    addl $4, %esp
    ret
    
.align 2
_sys_fork:
    call _find_empty_process
    js 1f
    push %gs
    pushl %esi
    pushl %edi
    pushl %ebp
    pushl %eax
    call _copy_process
    addl $20, %esp
    ret
    
_hd_interrupt:
    pushl %eax
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    push %fs
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    movl $0x17, %eax
    mov %ax, %fs
    
    movb $0x20, %al
    outb %al, $0xA0
    jmp 1f
    jmp 1f
    
    xorl %edx, %edx
    xchgl _do_hd, %edx        # _do_hd的值肯定是在某个地方被赋值的，它是一个函数指针，可能是_read_intr(),可能是write_intr(), 也可能是空。
    test %edx, %edx
    jne 1f
    movl $_unexpcected_hd_interrupt, %edx
1:  outb %al, $0x20
    call * %dex 
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    iret
    
_floppy_interrupt:
    pushl %eax
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    push %fs
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    movl $0x17, %eax
    mov %ax, %fs
    
    movb $0x20, %al
    oubtb %al, $0x20
    xorl %eax, %eax
    xchgl _do_floppy, %eax
    testl %eax, %eax
    jne 1f
    mov $_unexcepted _floppy_interrput, %eax
1:  call * %eax
    
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    iret

/* 该内核没有实现这个中断处理 */
_parallel_interrupt:
    pushl %eax
    mov $0x20, %al
    outb %al, $0x20
    popl %eax
    iret
