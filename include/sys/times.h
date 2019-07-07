#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <sys/types.h>
struct tms
{
	time_t tms_utime;	// 用户使用cpu的时间
	time_t tms_stime;	// 系统使用cpu的时间
	time_t tms_cutime;	// 已经终止子进程使用的用户cpu时间
	time_t tms_cstime;	// 已经终止子进程使用的系统cpu时间
};

extern time_t times(struct tms* tp);

#endif	// _SYS_TIMES_H
