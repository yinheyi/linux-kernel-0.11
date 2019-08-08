#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long addr);
long last_pid = 0;

/**
* @brief 核实从addrss开始的size个字节都分配了内存页，如果没有分配，则进行分配。
* @param [in] addr 起始地址
* @param [in] size 以字节为单位的空间大小
* @return 返回为空。
*
* 开启了分页机制之后，一个页为4096字节，所以内存每一次也是增加一页。addr的地址很可能没有与页对齐，
* 所以呢，本函数对addr向下取整页，把不够一页的字节大小加到size上，然后调用write_verify()获取对size
* 向上取整的页数。
*/
void verify_area(void* addr, int size)
{
  unsigned long start = (unsigned long)addr;
  size += start & 0xfff;
  start &= 0xfffff000; 
  start += get_base(current->ldt[2]);
  while (size > 0)
  {
    size -= 4096;
    write_verify(start);
    start += 4096;
  }
}

/**
*
*/
