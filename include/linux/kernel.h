/**
  该文件定义了一些内核常用的函数原型等。
*/

#ifndef _KERNEL_H
#define _KERNEL_H

void verify_area(void* addr, int count);
volatile void panic(const char* str);
int printf(const char* fmt, ...);
int printk(const char* fmt, ...);
int tty_write(unsigned ch, char* buf, int count);
void* malloc(unsigned int size);
void free_s(void* obj, int size);

#define free(x) fress_s((x), 0);
#define suer() (current->euid == 0)

#endif //_KERNEL_H
