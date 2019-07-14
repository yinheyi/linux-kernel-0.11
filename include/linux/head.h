#ifndef _HEAD_H
#define _HEAD_H
typedef struct desc_struct		// 描述符占8个字节
{
	unsigned long a;
	unsigned long b;
} desc_table[256];

extern unsigned long pg_dir[1024];
extern desc_table idt, gdt;

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif // _HEAD_H
