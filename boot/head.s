.text
.globl _idt, _gdt, _pg_dir, _tmp_floppy_area
_pg_dir:
startup_32:
	mov1 $0x10, %eax	! 保护模式下，段寄存器中存放的是选择子。
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	lss _stack_start, %esp
	call setup_idt
	call setup_gdt
	mov1 $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	lss _stack_start, %esp

! 测试A20地址线的代码，不需要管, 都过时了。
	xor1 %eax, %eax
1:	incl %eax
	movl %eax, 0x000000
	cmpl %eax, 0x100000
	je 1b

! 检测数学协处理器是否存在, 也不需要字，都过时了。
	movl %cr0, %eax
	andl $0x80000011, %eax
	orl $2, %eax
	movl %eax, %cr0
	call check_x87
	jmp after_page_tables
check_x87:
	fninit
	fstsw %eax
	cmpb $0, %al
	je 1f
	movl %cr0, %eax
	xorl $6, %eax
	movl %eax, %cr0
	ret
.align 2
1: .byte 0xDB, 0xE4
	ret

setup_idt:
	! 构建中断描述符， eax保存低32位，edx保存高32位。
	lea ignore_int, %edx
	movl $0x0080000, %eax
	movw %dx, %ax
	movw $0x8E00, %dx
	lea _idt, %edi
	mov $256, %ecx
rp_sidt:
	mov1 %eax, (%edi)
	movl %edx, 4(%edi)	! 4表示在edi + 4
	addl $8, %edi
	dec %ecx
	jne rp_sidt
	lidt idt_descr
	ret

setup_gdt:
	lgdt gdt_descr
	ret

! 4个页目录表
.org 0x1000
pg0:
.org 0x2000
pg1:
.org 0x3000
pg2:
.org 0x4000
pg3:

.org 0x5000
_tmp_floppy_area:
	.fill 1024, 1, 0

after_page_tables:
	pushl $0
	pushl $0
	pushl $0
	pushl $L6
	pushl $_main		! 把main.c函数的地址塞进来了。
	jmp setup_paging
L6:
	jmp L6

int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2

! 默认的中断处理程序
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, fs
	pushl $int_msg
	call _printk
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

.align 2
setup_paging:
	! 把页目录和页表清0
	movl  $1024 * 5, %ecx
	xorl %eax, %eax
	xorl %edi, %edi
	cld
	rep
	stosl

	! 设置页目录中的四个页表
	movl $pg0 + 7, _pg_dir
	movl $pg1 + 7, _pg_dir + 4
	movl $pg2 + 7, _pg_dir + 8
	movl $pg3 + 7, _pg_dir + 12

	! 接下来，设置4个页表内的页表项
	movl $pg3 + 4092, %edi
	movl $0xfff007, % eax		!从这里可以看出来，逻辑地址到物理地址之间是线性映射的。
	std			! 该命令置DF位为1
1:	stosl
	subl $0x1000, %eax
	jbg 1b

	! 设置目录基址寄存器(CR3)的值。
	xor1 %eax, %eax
	movl %eax, %cr3

	! 设置cr0里的第31位为1, 开启分页。
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0
	ret

.align 2
.word 0
idt_descr:
	.word 256 * 8 - 1
	.long _idt
	
.align 2
.word 0
dgt_descr:
	.word 256 * 8 - 1
	.long _gdt
	
.align 3
_idt:
	.fill 256, 8, 0		! 填充256个8字节的0值, 该内存空间在setup_idt子程序中被填进中断描述符
! 全局描述符表。
_gdt:
	.quad 0x0000000000000000
	.quad 0x00c09a0000000fff
	.quad 0x00c0920000000fff
	.quad 0x0000000000000000
	.fill 252, 8, 0
