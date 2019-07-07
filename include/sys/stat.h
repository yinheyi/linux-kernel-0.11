// 该文件说明了函数stat()返回的数据类型等。
#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>
struct stat
{
	dev_t st_dev;
	ino_t st_ino;	// 文件节点
	mode_t st_mode;	// 文件属性
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	time_t st_atime;	// access time
	time_t st_mtime;	// modify time
	time_t st_ctime;	// 最后节点修改时间
};

// 文件类型
#define S_IFMT 00170000		// 文件类型，8进行表示, 它的使用就是与具体一个数进行与操作，把与文件类型无关的位置0.
#define S_IFREG 0100000		// 常规文件
#define S_IFBLK 0060000		// 块文件
#define S_IFDIR 0040000		// 目录文件
#define S_IFCHR 0020000		// 字符文件
#define S_IFIFO 0010000		// 特殊文件

// 文件属性
#define S_ISUID	0004000
#define S_ISGID 0002000
#define S_ISVTX 0001000

// 宏定义的测试 , 判断m是否是上面定义的某一文件类型
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

// 宿主对文件的权限
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

// 组成员对文件的权限
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

// 其它人对文件的权限
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

// 文件操作的相关函数声明
extern int chmod(const char* _path, mode_t mode);
extern int fstat(int fildes, struct stat* stat__buf);	// 取指定文件句柄的文件状态信息
extern int mkdir(const char* _path, mode_t mode);
extern int mkfifo(const char* _path, mode_t mode);
extern int stat(const char* filename, struct stat* stat_buf);	// 取指定文件的文件状态信息
extern mode_t umask(mode_t mask);

#endif //_SYS_STAT_H
