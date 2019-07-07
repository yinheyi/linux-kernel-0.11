// 该文件定义了与时间相关的类型和数据结构
#ifndef _TIME_H
#define _TIME_H

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;		// 以秒为单位的时间数(从1970年1月1日开始的计数)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef long clock_t	 // 系统经过的时钟滴答数
#endif

#define CLOCKS_PER_SEC 100

struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_day;
	int tm_mon;
	int tm_year;
	int tm_wday;		// 一星期中的哪一天
	int tm_yday;		// 一年中的哪一天
	int tmm_isdst;		// 夏时令
}

// 时间相关的操作函数
clock_t clock(void);
time_t time(time_t* tp);		// 为什么还传入一个指针？
double difftime(time_t time2, time_t time1);
time_t mktime(struct tm* tp);
char* asctime(const struct tm* tp);
char* ctime(const time_t* tp); 
struct tm* gmtime(const time_t* tp);
struct tm* localtime(const time_t* tp);
size_t strftime(char* s, size_t smax, const char* fmt, const struct tm* tp);
void tzset(void);

#endif // _TIME_H
