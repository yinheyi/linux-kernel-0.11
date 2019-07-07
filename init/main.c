#define _LIBRARY_		// 定义该宏目的是为了包含<unistd.h>内的一部分内容
#include <unistd.h>
#include <time.h>

static inline systemcall0(int,fork);
static inline systemcall0(int,pause);
static inline systemcall0(int,sync);
static inline systemcall1(int,setup,void*,BIOS);

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>
#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, int length);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm* tm);
extern long startup_time;

// 下面几行代码内的地址内的数据是在setup.s中存入的。
#define EXT_MEM_K (*(unsigned short*)0x90002)
#define DRIVE_INFO (*(struct drive_info*)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short*)0x901FC)

// 读取CMOS的时钟信息的宏定义
#define CMOS_READ(addr) ( \
	outb_p(addr, 0x70); \
	inb_p(0x71);\
)

// BCD码的转换
#define BCD_TO_BIN(val) ((val) = ((val)&15) + ((val)>>4) * 10)

// 从CMOS读取相关信息，初始化内核时间
static void time_init(void)
{
	struct tm time;
	do
	{
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));

	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	--time.tm_mon;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;
struct drive_info
{
	char dummy[32];
} drive_info;

void main(void)
{
	ROOT_DEV = ORIG_ROOT_DEV;
	drive_info = DRIVE_INFO;
	memory_end = (1 << 20) + (EXT_MEM_K << 10);		// 1M + 扩展内存(kb)
	memory_end &= 0xfffff000;		// 1000为4KB,正好可以表示一页，这里把不足一页的内存忽略掉。
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;
	if (memory_end > 12 * 1024 * 1024)
		buffer_memory_end = 4 * 1024 * 1024;
	else if (memory_end > 6 * 1024 * 1024)
		buffer_memory_end = 2 * 1024 * 1024;
	else
		buffer_memory_end = 1 * 1024 * 1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK * 1024);
#endif

	// 内核初始化工作
	mem_init(main_memory_start, memory_end);
	trap_init();
	blk_dev_init();
	chr_dev_init();
	tty_init();
	time_init();
	sched_init();
	buffer_init(buffer_memory_end);
	hd_init();
	floppy_init();
	sti();		// 开启中断
	move_to_user_mode();

	// 从下面if语句开始，要分成两个进程了。
	if (!fork())
	{
		init();
	}
	else
	{
		while (1)
			pause();
	}
}

static int printf(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int i = vsprintf(printbuf, fmt, args);
	write(1, printbuf, i);
	va_end(args);

	return i;
}

static char* argv_rc[] = {"/bin/sh", NULL};
static char* envp_rc[] = {"HOME=/", NULL};
static char* argv[] = {"/bin/sh", NULL};
static char* envp[] = {"HOME=/usr/root", NULL};

void init(void)
{
	int pid, i;
	setup((void*) &drive_info);
	(void) open("/dev/tty0", O_RDWR, 0);		// 加 (void)的目的何在？
	(void)dup(0);
	(void)dup(0);
	printf("Free mem: %d bytes\n\r", memory_end - main_memory_start);

	if (!(pid = fork()))
	{
		close(0);
		if (open("/etc/rc", O_RDONLY, 0))
			_exit(1);
		execve("/bin/sh", argv_rc, envp_rc);
		_exit(2);
	}

	if (pid > 0)
	{
		while (pid != wait(&i))
		{}
	}

	while (1)
	{
		if ((pid  = fork()) < 0)
		{
			printf("Fork failed in init.\r\n");
			continue;
		}

		if (!pid)
		{
			close(0);
			close(1);
			close(2);
			setsid();

			(void) open("/dev/tty0", O_RDWR, 0);
			(void)dup(0);
			(void)dup(0);
			_exit(execve("/bin/sh", argv, envp));
		}

		while (1)
		{
			if (pid == wait(&i))
				break;
		}

		printf("\n\r child %d died with code %04x \n\r", pid, i);
		sync();
	}
	_exit(0);
}
