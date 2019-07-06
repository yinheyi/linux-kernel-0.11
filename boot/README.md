boot相关的知识说明：  
## 中断相关
### 中断int0x10 相关功能说明  
#### 功能一： 读取光标位置和扫描行大小
- **输入参数说明：**
```
AH = 0x03, 03表示该功能号。
BH = 表示vedio page
```
- **返回值说明:**
```
CH = 光标扫描的起始行(低5位有效)
CL = 光标扫描的终止行(低5位有效)
DH = 光标所在的行
DL = 光标所在的列
```

#### 功能二： 向屏幕上写字符串
- **输入参数说明：**
```
AH = 0x13H, 表示该功能号
AL = 控制写的模式, 具体为：
		= 0 string is chars only, attribute in BL, cursor not moved
		= 1 string is chard only, attribute in BL, cursor moved
		= 2 string contains chars and attributes, cursor not moved
		= 3 string contains chars and attributes, cursor moved
BH = vedio page number
BL = 属性信息（当AL取0或1模式时有效)
CX = 要读取的字符串的长度（该长度是忽略字符属性信息的)
DH = 用于指出行的坐标
DL = 用于指出列的坐标
ES:BP = 指向内存中字符串的位置
```
- **返回值说明:**
```
无
```

#### 功能三： 获取显示模式
- **输入参数说明：**
```
AH = 0x0f, 表示该功能号
```
- **返回值说明:**
```
AH =  屏幕上显示的字符列数
AL =  当前的显示模式
BH =  当前的显示页
```

#### 功能四： 获取显示的配置信息
- **输入参数说明：**
```
AH = 0x12, 表示该功能号
BL = 10
```
- **返回值说明:**
```
BH = 当为彩色模式时, 为0；当为单色模式时为1.
BL = 显存为64kb/128kb/192kb/256/kb时，分别为0/1/2/3
CH = feature bits
CL = switch settings
```

### 中断int 0x13
0x13中断号的地址存放的是BIOS提供的软盘服务例程，提供了对软盘/磁盘的相关操作, 功能介绍可以[点击这里](http://stanislavs.org/helppc/int_13.html)  
下面仅仅例举出内核引导启动程序使用到的功能：  
#### 功能一： 读取磁盘扇区到指内的内存地址  
- **输入参数说明:**  
```
AH = 2, 在AH中的值表示int 0x13的功能号，2表示读扇区功能  
AL = 要读取扇区的数量  
CH = 表示磁道号  
CL = 扇区号  
DH = 磁头号  
DL = 驱动器号， 软驱从0x00开始，00表示软驱0, 01表示软驱1;  硬盘从0x80开始(即第七位置1）， 0x80表示硬盘0, 0x81表示硬盘2  
ES:BX = 使用es段地址和bx基地址来表示要读取到的内存缓冲区  
```

- **返回值说明:**  
```
AH = 表示返回的状态码,具体为：  
		00  no error
		01  bad command passed to driver
		02  address mark not found or bad sector
		03  diskette write protect error
		04  sector not found
		05  fixed disk reset failed
		06  diskette changed or removed
		07  bad fixed disk parameter table
		08  DMA overrun
		09  DMA access across 64k boundary
		0A  bad fixed disk sector flag
		0B  bad fixed disk cylinder
		0C  unsupported track/invalid media
		0D  invalid number of sectors on fixed disk format
		0E  fixed disk controlled data address mark detected
		0F  fixed disk DMA arbitration level out of range
		10  ECC/CRC error on disk read
		11  recoverable fixed disk data error, data fixed by ECC
		20  controller error (NEC for floppies)
		40  seek failure
		80  time out, drive not ready
		AA  fixed disk drive not ready
		BB  fixed disk undefined error
		CC  fixed disk write fault on selected drive
		E0  fixed disk status error/Error reg = 0
		FF  sense operation failed 
AL = 成功读取的扇区数目
CF = 如果操作成功，为0, 如果失败则置1
 ```
#### 功能二： 磁盘复位功能
- **输入参数说明：**  
```
AH = 0x00, 0表示磁盘的复位功能号
DL = 驱动器号
```
- **返回值说明：**  
```
AH = 磁盘操作状态的码
CF = 如果成功则为0, 如果失败为置位1。
```

#### 功能三：读取磁盘驱动器参数
- **输入参数说明：**
```
AH = 0x08, 08表示获取当前驱动参数功能
DL = 驱动器号
```
- **返回值说明:**  
```
AH = 磁盘操作状态的码
BL = CMOS驱动器的类型
CH = 磁道数的低8位
CL = 0～5位表示第一个磁道的扇区数， 6～7位表示磁道数的高2位
DH = 磁头数/磁面数
DL = 驱动器数目
ES:DI = 指向11个字节的DBT(disk base table), 即该中断功能会修改es和di寄存器的值。DBT表存放了一些信息，具体如下：

		Offset Size		Description

		00   byte  specify byte 1; step-rate time, head unload time
		01   byte  specify byte 2; head load time, DMA mode  
		02   byte  timer ticks to wait before disk motor shutoff
		03   byte  bytes per sector code:
				0 - 128 bytes	2 - 512 bytes
				1 - 256 bytes	3 - 1024 bytes
		04   byte  sectors per track (last sector number)
		05   byte  inter-block gap length/gap between sectors
		06   byte  data length, if sector length not specified
		07   byte  gap length between sectors for format
		08   byte  fill byte for formatted sectors
		09   byte  head settle time in milliseconds
		0A   byte  motor startup time in eighths of a second
		.
CF = 如果成功则为0, 如果失败为置位1。
```

#### 功能四： 读取存储器的类型
- **输入参数说明：**  
```
AH = 0x15, 表示该功能号。
DL = 驱动器号，（0x00为软盘0, 0x01为软盘1...,  0x80为硬盘0, 0x81为硬盘1......)
```
- **返回值说明：**  
```
AH = 00 ,表示不存在存储器
   = 01, 表示为软盘，no change detection present
   = 02, 表示为软盘， change detection present
   = 03, 为fixed dist, 即硬盘
CX:DX = 当AH的值为3时，cx:dx保存了硬盘的扇区数.
CF = 如果成功则为0, 如果失败为置位1。
```

###  中断int0x15 相关功能说明  
#### 功能一： 获取扩展内存的大小 
该指令只能工作在80286和80386的机器上。  
- **输入参数说明：**  
```
AH = 0x88, 表示该功能号。
```
- **返回值说明：**  
```
CF =  0x80, for PC, PCjr
   =  0x86, for XT and Modle 30
   =  对于其它机器，如果出错则置1,否则为0.
AX = 从0x100000(即1024Kb)处开始算起的内存块数目（kb为单位)
```

## intel 8086汇编编译器相关语法说明  
### 转移相关指令
- je 指令：	jump equal, 当Zero Flag 置1时，跳转
- jz 指令： jump zero, 与je相同, 当Zero Flag 置1时，跳转
- jne 指令: jump not equal, 当Zero Flag为0时，执行跳转
- jnz 指令：jump not zero, 	当zero Flag为0时，执行跳转
- jnc 指令：jump not carry, 当Carry Flag为0时，执行跳转
- jmpi 指令： 段间跳转指令， cs和ip寄存器的值会更新。

### 移位指令
- shl 指令： shift logical left, 逻辑左移, 会修改标志位寄存器， CF中保存了最后一个被移出的位.
- sal 指令： shift arthmetic left, 算术左移, 与逻辑左移没有区别
- shr 指令： shift logical right, 逻辑右移, 会修改标志位寄存器， CF中保存了最后一个被移出的位.
- sar 指令： shift arthmetic right, 算术右移，与逻辑右移的区别在于最左边位使用符号位补齐。

### 复制指令
- movsb 指令： move byte, 该指令从DS:SI复制一个字节到ES:DI， 并更新si和si的值(使用DF控制递增还是递减）。 该指令经常和REP一起使用。
- movsw 指令： move word, 该指令从DS:SI复制一个字到ES:DI， 并更新si和si的值(使用DF控制递增还是递减）。 该指令经常和REP一起使用。
- stos(b,w,d)  指令： store string(byte,word, double word),  该指令把ax中的值存储到es:di指向的内存空间，并更新di的值。 该指令也经常和REP一起使用。  
- movx 指令： AT&T的指令, 当x为l时，表示复制32位的值; 当x为w时，表示复制16位的值; 当x为b时，表示复制8位的值。

### 其它
- LDS 指令： load pointer using ds, 使用方法：LDS dest, src  该指令把src指向的内存中的32地址载入到ds寄存器(段地址，高16位)和dest寄存器(偏移地址, 低16位）中.  
- LSS 指令： load pointer using SS, 与LDS类似，只不过寄存器换成了SS.
- LEA 指令： load effective address, 把src的偏移地址载入到des寄存器中。
- CLI 指令： Clear Interrupt Flag, 把IF位清0, 禁止掉硬件中断
- CLD 指令： clear direction flag, 把DF位清0, 用于控制movs相关指令的si/di为递增。
- lidt 指令： 加载6字节值到中断描述符表寄存器中。
- lgdt 指令： 加载6字节值到全局描述符表寄存器中。
- lmsw 指令： load machine status word,机器状态字说明：0位保护模式位，1位为Math Present, 2位为Emulation, 3位为Task Switched, 4位为扩展位， 5-30位为保留位，31位为Paging
