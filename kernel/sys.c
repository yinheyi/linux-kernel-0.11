/**
* @file sys.c
* @brief 本文件主要包含了系统调用的实现函数。
*/

#include <errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

/**
  @brief 设置当前进程的rgid和egid.
  @param [in] rgid 要设置的实际gid.
  @param [in] egid 要设置的有效gid.

  1. 当有超级用户权限时，会把gid设置为rgid.
  2. 当有超级用户权限时或者进程的gid == egid时，或者进程的sgid == egid时，把进程的egid设置为egid.
*/
int sys_setregid(int rgid, int egid)
{
	if (rgid > 0)
	{
		// 真的看是明白这个if语句啊？为什么判断语句中多加一个current->gid == rgid呢？
		// 意思何在？看上去，是多余的。
		if ((current->gid == rgid) || suser())
			current->gid = rgid;
	}
	else
		return -EPERM;

	if (egid > 0)
	{
		// 两样的疑问？在if判断语句中，为什么多加一个current->egid == egid呢？
		if ((current->gid == egid) || (current->egid == egid)
			|| (current->sgid == egid) || suser())
			current->egid = egid;
		else
			return -EPERM;
	}
	return 0;
}

/**
  @brief  没有看明白啊。
*/
int sys_setgid(int gid)
{
	return sys_setregid(gid, gid);
}

/**
  @brief 返回从1970.1.1 00:00:00 开始的秒数
  @param [in] tolc 指针类型，如果tolc不为空，把时间这到该指针指向的位置。
  @return 返回当前的时间。

  该函数直接从CURRENT_TIME中取得了时间。
*/
int sys_time(long* tloc)
{
	int i;
	i = CURRENT_TIME;

	// 如果指针tloc不为空，也把时间放到那里一份。
	if (tloc)
	{
		verify_area(tloc, 4);
		put_fs_long(i, (unsigned long*)tloc);
	}
	return i;
}

/**
  @brief 和sys_setregid()函数类似，看着有点迷惑啊，不太明白。
*/
int sys_setreuid(int ruid, int euid)
{
	int old_ruid = current->uid;

	if (ruid > 0)
	{
		if (current->euid == ruid || old_ruid == ruid || suser())
			current->uid = ruid;
		else
			return -EPERM;
	}

	if (euid > 0)
	{
		if (old_ruid == euid || current->euid == euid || suser())
			current->euid = euid;
		else
		{
			current->uid = old_ruid;
			return -EPERM;
		}
	}
	return 0;
}

/**
  @brief 看不明白了，不先写了。
*/
int sys_setuid(int uid)
{
}


/**
  @brief 设置系统启动时间。
  @param [in] tptr timeptr,它传递了从1970年开始的计时时间(以秒为单位)
  @return 成功返回0, 失败返回-EPERM

  具体实现：设置startup_time的值。
*/
int sys_stime(long* tptr)
{
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long*)tptr) - jiffies / HZ;
	return 0;
}

/**
  @brief 获取当前里程的时间，包括用户时间，系统时间，子里程的用户时间，子进程的系统时间.
  @param [in] tbuf 它是一个结构体指针，用于传出与进程相关的时间。
  @return 返回当前的嘀嗒声。
*/
int sys_times(struct tms *tbuf)
{
	if (tbuf)
	{
		verify_aera(tbuf, sizeof *tbuf);
		put_fs_long(current->utime, (unsigned long*)&tbuf->tms_utime);
		put_fs_long(current->stime, (unsigned long*)&tbuf->tms_stime);
		put_fs_long(current->cutime, (unsigned long*)&tbuf->tms_cutime);
		put_fs_long(current->cstime, (unsigned long*)&tbuf->tms_cstime);
	}
	return jiffies;
}
