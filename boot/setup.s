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
