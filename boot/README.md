boot相关的知识说明：  
### 1. 中断int 0x13使用到的相关功能说明：
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

### 2. 中断int10 相关功能说明  
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
