#ifndef _SEGMENT_H
#define _SEGMENT_H

/** \brief 读取fs段中指定地址处的一个字节
* @param [in] addr 段内的偏移地址
* @return 返回一个字节
*/
extern inline unsigned char get_fs_byte(const char* addr)
{
	unsigned register char _v;
	__asm__("movb %%fs:%1, %0"
			:"=r"(_v)
			:"m"(*addr));
	return _v
}

/** \brief 读取fs段中指定地址处的一个字
* @param [in] addr 段内的偏移地址
* @return 返回一个字
*/
extern inline unsigned char get_fs_word(const unsigned short* addr)
{
	unsigned short _v;
	__asm__("movw %%fs:%1, %0"
			:"=r"(_v)
			:"m"(*addr));
	return _v
}

/** \brief 读取fs段中指定地址处的两个字
* @param [in] addr 段内的偏移地址
* @return 返回两个字
*/
extern inline unsigned long get_fs_long(const unsigned long* addr)
{
	unsigned long _v;
	__asm__("movl %%fs:%1, %0"
			:"=r"(_v)
			:"m"(*addr));
	return _v;
}

/** \brief 把一个字节放到fs段中的指定位置 
* @param [in] val 一个的字节的值。
* @param [in] addr 要存放的地址。
*/
extern inline void put_fs_byte(char val, char* addr)
{
	__asm__("movb %0, %1"
			::"r"(val), "m" (*addr));
}

/** \brief 把一个字放到fs段中的指定位置 
* @param [in] val 一个的字的值。
* @param [in] addr 要存放的地址。
*/
extern inline void put_fs_word(short val, short* addr)
{
	__asm__("movw %0, %1"
			::"r" (val), "m" (*addr));
}

/** \brief 把两个字放到fs段中的指定位置 
* @param [in] val 两个字的值。
* @param [in] addr 要存放的地址。
*/
extern inline void put_fs_long(long val, long* addr)
{
	__asm__("movl %0, %1"
			::"r"(val), "m"(*addr));
}

/** \brief 获取fs寄存器的值。*/
extern inline unsigned long get_fs()
{
	unsigned long _v;
	__asm__("movl %%fs, %%ax"
			:"=a" (_v)
			::"eax");
	return _v;
}

/** \brief 获取ds寄存器的值。*/
extern inline unsigned long get_fs()
{
	unsigned long _v;
	__asm__("movl %%ds, %%ax"
			:"=a" (_v)
			::"eax");
	return _v;
}

/** \brief 设置fs段寄存器。 */
extern inline void set_fs(unsigned long val)
{
	__asm__("mov %0, %%fs"
			::"r" ((unsigned short)val));
}

#endif //_SEGMENT_H
