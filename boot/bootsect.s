SYSSIZE = 0x3000
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4		! setup模块在磁盘上占的扇区数, 读取它时会使用到。
BOOTSEG = 0x07c0	!bootsect的段地址，bios会把第一扇区的内容读到0x7c00处，所以此处设置段地址为0x07c0.
INITSEG = 0x9000	!把自己移动到0x90000处，此处新段地址设置为0x9000
SETUPSEG = 0x9020	!setup.s在内存中的位置为0x90200,这是它的段地址。
SYSSEG = 0x1000 	!会把system模块读到0x10000处，这是它的段地址。
ENDSEG = SYSSEG + SYSSIZE

ROOT_DEV = 0x306

entry start

! 程序最开始，把自己从0x7c00处移动到0x9000处。
start:
	! 把自身512个字节从一开始的0x7c00处移动到0x9000处, 即从ds:si移动到es:di处。
	mov ax, #BOOTSEG	!立即数为0x07c0, 段地址
	mov ds, ax
	mov ax, #INITSEG
	mov es, ax
	sub si, si			! 把si置为0
	sub di, di
	mov cs, #256
	rep
	movw
	jmpi go, INITSEG	! 段间跳转，会设置cs为INITSEG, ip为go标号的段内偏移值。

go:
	mov ax, cs			! 把数据段/栈段/es都设置为与cs相同，即0x9000。
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov sp, #oxff00		! 只要栈的空间不要覆盖掉setup的空间就行，setup在ox90200处，它的大小大约2kB.

! 接下来，加载setup模块
load_setup:
	! 接下来，借助int 0x13中断（bios提供了中断处理函数用于进行磁盘操作)从磁盘第2扇区读到setup模块到0x90200处。
	mov ax, #0x0200+SETUPLEN
	mov bx, #0x0200		!把setup模块读取到内存地址为0x90200处。
	mov cx, #0x0002
	mov dx, #0x0000
	int 0x13			! 使用中断0x13进行读取操作
	jnc ok_load_setup	! 条件跳转，如果cf位为0,表示没有出错，即跳转的ok_load_)setup处去执行。
	mov dx, #0x0000		! 设置磁盘复位的参数
	mov ax, #0x0000		! 设置磁盘复位的参数
	int 0x13			! 磁盘进行复位 
	j load_setup		! 跳转到load_setup处，重新加载

ok_load_setup:
	!使用int0x13的读取磁盘驱动器参数的功能读取每一个磁道的扇区数
	mov dl, #0x00
	mov ax, #0x0800
	int 0x13
	mov ch #0x00		!把高8位清空为0, ch的值为磁道数的位8位的值，这里不需要它的值。
	seg cs 				! 该指令仅仅会影响到下条指令中使用到的段寄存器
	mov sectors, cx 	! 直接把cx的值复制到代码段内偏移为secotrs的内存处，本来cl的和6位和第7位表示的是磁道数的高2位,但并没有清空，
						! 我猜测对于软盘来说，只有80个磁道，它的高2位肯定为0了，所以cx的值表示了每一个磁道内的扇区数。
	mov ax #INITSEG
	mov es, ax			! int 0x13, 8 功能会修改es:di寄存器的值，这里把它们还原回来。

	! 获取光标的位置信息，执行中断程序之后，位置信息会存放在DH和DL中， 供接下来写向屏幕上写字符串使用。
	mov ah, #0x03
	xor bh, bh 			! bh的值表示第为页的vedio page
	int 0x10

	! 向屏幕上打印字符串:"loading system..."
	mov ax, #0x1301
	mov bx, #0x0007
	mov cx, #24
	mov bp, #msg1
	int 0x10

	!接下来，从磁盘中读取system模块到内存0x10000处。
	mov ax, #SYSSEG
	mov es, ax
	call read_it
	call kill_motor

! 接下来，就是检测根文件系统设备号的,即是确定root_dev的值。
	seg cs
	mov ax, root_dev
	cmp ax, #0
	jne root_defined:
	seg cs
	mov bx, sectors
	mov ax, #0x0208		！1.2MB的设备号, 它的磁道中的15个扇区
	cmp bx, #15
	je root_defined
	mov ax, #0x021c		! 1.44MB软件的设备号，它的磁道中有18个扇区
	cmp bx, #18
	je root_defined
undef_root:
	jmp undef_root
root_defined:
	seg cs
	mov root_dev, ax

! 控制权转移到setup中
jmpi 0, SETUPSEG

sread: .word 1+SETUPLEN		!用于保存当前磁道上已经读取的扇区数
head: .word 0		! 用于保存当前正在读取的磁头
track: .word 0		! 用于保存当前的磁道
sectors: .word 0	! 用于保存当前一个磁道上的扇区数

read_it:
	mov ax, es
	test ax, #0x0fff	!test指令对两个操作数执行逻辑与， 结果仅仅会修改标志寄存器的值。此处用于检测es的值为0x1000
die:
	jne die				! 如果ZF标志位为0,则执行标号处的指令， 即上面的test测试结果如果不为0,则ZF位会被clear,从而进入死循环。
	xor bx, bx
rp_read:
	mov ax, es
	cmp ax, #ENDSEG		!cmp指令类似sub指令，cmp只影响flags寄存口器的值。
	jb ok1_read			! 如果CF置位，即跳转
	ret
ok1_read:
	seg cs
	mov ax, sectors
	sub ax, sread
	mov cx, ax
	shl cx, #9
	add cx, bx		! bx为段内数据的偏移值
	jnc ok2_read	! 小于等于64kb时，执行ok2_read.之所以是64kb,因为实模式下段内的偏移最大就是64kb
	je ok2_read
	xor ax, ax
	sub ax, bx
	shr ax, #9
ok2_read:			! 此时ax的值为需要读取的扇区数
	call read_track
	mov cx, ax
	add ax, sread
	seg cs
	cmp ax, sectors
	jne ok3_read
	mov ax, #1
	sub ax, head
	jne ok4_read	!此时说明了1号磁头还没有读取，跳转去读取1号磁头
	inc track
ok4_read:
	mov head, ax
	xor ax, ax
ok3_read:			! 进入这里时，ax保存了当前磁道已经读取的扇区数，cx也是保存了当前已经读取的扇区数
	mov sread, ax
	shl cx, #9
	add bx, cx
	jnc rp_read
	mov ax, es				! 当超过了64kb时, 即b溢出了，调整段寄存器的值，增加64kb,再继续读取
	mov ax, #0x1000
	mov es, ax
	xor bx, bx
	jmp rp_read
read_track:			! ax中保存了需要读取的扇区数, read_track是真正执行读取操作的代码
	push ax
	push bx
	push cx
	push dx
	mov cx, sread	! sread的值是已经读取的扇区号，加1之后表示要读取的扇区号。
	inc cx
	mov dx, track
	mov ch, dl
	mov dx, head
	mov dh, dl
	mov dl, #0
	and dx, #0x0100		!碰头号不大于1(对于软盘就两个磁头）
	mov ah, #2
	int 0x13
	jc bad_rt			! 读取失败时，CF会置位
	pop dx
	pop cx
	pop bx
	pop ax

bad_rt:
	mov ax, #0
	mov dx, #0
	int 0x13
	pop dx
	pop cx
	pop bx
	pop ax
	jmp read_track

kill_motor:
	push dx
	mov dx, #0x3f2		! dx指令的端口号
	mov al, #0
	outb
	pop dx
	ret

msg1:
	.byte 13, 10		! 回车与换行的ascii的值。
	.ascii "Loading system ..."
	.byte 13, 10, 13, 10

.org 508		! 伪指令，表示从地址508开始, 加上下面的两个字，正好是512字节 。
root_dev:
	.word ROOT_DEV
	.word 0xaa55

.text
endtext:
.data
enddata:
.bss
endbss:
