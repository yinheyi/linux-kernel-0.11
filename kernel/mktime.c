#include <time.h>

#define MINUTE 60
#define HOUR (60 * MINUTE)
#define DAY = (24 * HOUR)
#define YEAD = (365 * DAY)

/* 每一个月开始的秒数， 按闰年计算的，即二月份按29天 */
static  int month[12] = {
0，
DAY * (31),
DAY * (31 + 29),
DAY * (31 + 29 +31),
DAY * (31 + 29 +31 + 30),
DAY * (31 + 29 +31 + 30 + 31),
DAY * (31 + 29 +31 + 30 + 31 + 30),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31 + 31),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31 + 31 + 30),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30),
DAY * (31 + 29 +31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31)
};

/** @brief  该函数计算从1970年1月1日开始至开机时经过的秒数。
* @param [in] tm 时间结构体的指针，里面包含了从CMOS中读取到的开机时间。
* @return long类型 从1970年1月1日开始至开机时经过的秒数。
*
* 该函数好像没有考虑被100整除的年份可能不是闰年的情况。
/
long kernel_mktime(struct tm* tm)
{
    long res;
    int year;
    
    year = tm->tm_year - 70;
    res = YEAR * year + DAY * ((year + 1) / 4);        // 1972年是闰年，从1973年开始算起，每过4年都要加闰年我出来的一天。
    res = month[tm->tm_mon];
    if (tm->tm_mon > 1 && (year + 2) % 4))             // 如果当前的年为是闰年时，就减于多计算的那一天，因为month[12]是按闰年计算的。
        res -= DAY;
    res += DAY * (tm->tm_mday - 1);
    res += HOUR * tm->tm_hour;
    res += MINUTE * tm->tm_min;
    res += tm->tm_sec;
    
    return res;
}
