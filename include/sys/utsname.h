// 本文件定义了与系统名称相关的结构
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>
struct utsname
{
	char sysname[9];
	char nodename[9];
	char release[9];
	char version[9];
	char machine[9];
};

extern int uname(struct utsname* utsbuf);

#endif // _SYS_UTSNAME_H
