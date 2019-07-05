INITSEG = 0x9000
SYSSEG = 0x1000
SETUPSEG = 0x9020
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:
	mov ax, #INITSEG
	mov ds, ax		! 设置数据段, 待会会把读取到的信息放到这里面,该位置是原来bootsect.s的内存位置 。

	! 使用 int 0x10 中断读取光标的位置信息
	mov ah, 0x03
	xor bh, bh
	int 0x10
	mov [0], dx

	! 调用中断int 0x15, 获取扩展内存的大小。
	mov ah, #0x88
	int 0x15
	mov [2], ax

	! 调用中断int 0x10 , 获取显示模式
	mov ah, #0xf
	int 0x10
	mov [4], bx
	mov [6], ax

	!调用中断int 0x10, 获取显示配置信息
	mov ah, #0x12
	mov bl, #0x10
	int 0x10
	mov [8], ax		!ax的值是无意义的，它不是中断的返回值。不明白为什么要这么做。
	mov [10], bx
	mov [12], cx

	! 从中断向量表的0x41处获取第一个硬盘的信息的地址，并把该信息复制到内存地址0x90080处
	mov ax, #0x0000
	mov ds, ax
	lds si, [4*0x41]
	mov ax, #INITSEG
	mov es, ax
	mov di, #0x0080
	mov cx, #0x10
	rep
	movsb
	! 从中断向量表的0x46处获取第二个硬盘的信息的地址，并把该信息复制到内存地址0x90090处
	mov ax, #0x0000
	mov ds, ax
	lds si, [4*0x46]
	mov ax, #INITSEG
	mov es, ax
	mov di, #0x0090
	mov cx, #0x10
	rep
	movsb

	! 借助int 0x13中断检测是否真的存在第二块硬盘，不存在时，把存放第二块硬盘信息的地方清0.
	mov ax, 0x1500
	mov dl, #0x81
	int 0x13
	jc no_disk1
	cmp ah, #3
	je is_disk1
no_disk1:
	mov ax, #INITSEG
	mov es, ax
	mov di, #0x0090
	mov cx, #0x10
	mov ax, #0x00
	rep
	stosb

! 开始为进入保护模式作一起准备：
is_disk1:
	cli
	mov ax, #0x0000
	cld

!把system模块从0x10000处移动到0x0000处。
do_move:
	mov es, ax
	add ax, #0x1000
	cmp ax, #0x9000
	jz end_move
	mov ds, ax
	sub di, di
	sub si, si
	mov cx, #0x8000
	rep
	movsw
	jmp do_move

end_move:
	mov ax, #SETUPSEG	
	mov ds, ax
	lidt idt_48
	lgdt gdt_48

	! 开启A20地址线， 这个小部分不需要管，涉及到一些前景知识！
	call empty_8042
	mov al, #0xD1
	out #0x64, al
	call empty_8042
	mov al, #0xDF
	out #0x60, al
	call empty_8042

	! 进入保护模式
	mov ax, #0x0001
	lmsw ax
	jmpi 0, 8

empty_8042:
	.word 0x00eb, 0x00eb
	in al, #0x64
	test al, #2
	jnz empty_8042
	ret

gdt:
	.word 0, 0, 0, 0
	.word 0x07ff
	.word 0x0000
	.word 0x9a00
	.word 0x00c0

	.word 0x07ff
	.word 0x0000
	.word 0x9200
	.word 0x00c0
idt_48:
	.word 0
	.word 0, 0
gdt_48:
	.word 0x800
	.word 512+gdt, 0x9		!gdtr正好指向了gdt.

.text
endtext:
.data
enddata:
.bss
endbss:
