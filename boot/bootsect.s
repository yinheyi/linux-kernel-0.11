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

load_setup:
! 接下来，借助int 0x13中断（bios提供了中断处理函数用于进行磁盘操作)从磁盘第2扇区读到setup模块到0x90200处。
! 入口参数：
! ah 表示 int 0x13 中断的功能号， 2表示读扇区
! al 表示 读取扇区的总数
! ch 表示 磁道号的低8位
! cl 的0～5位表示开始的扇区号，6～7位表示 磁道号的高2位
! dh 表示 磁头号
! dl 表示 驱动器号， 软驱从0开始，即0, 1, 2... 硬盘从80开始(第7位为1)，即0x80, 0x81
! es:bs 指向了数据缓冲区
! 如果读取出错，则cf位会置1, cf位表示进位标志位
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

