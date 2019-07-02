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

