// 本文件定义了文件访问与修改时间的结构。
#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h>

struct utimbuf
{
	time_t actime;
	time_t modtime;
};

extern int utime(const char* filename, struct utimbuf* times);

#endif // _UTIME_H
