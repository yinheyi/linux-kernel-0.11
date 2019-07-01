SYSSIZE = 0x3000
.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4
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
