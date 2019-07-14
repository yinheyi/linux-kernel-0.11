//

// 模拟中断调用的返回过程来完成切换到任务0
#define move_to_user_mode()                                     \
__asm__("movl %%esp, %%eax\n\t"                                 \
		"pushl $0x17\n\t"                                       \
		"pushl %%eax\n\t"                                       \
		"pushfl\n\t"                                            \
		"pushl $0x0f\n\t"                                       \
		"pushl $1f\n\t"                                         \
		"iret\n\t"                                              \
		"1:\tmovl $0x17, %%eax\n\t"                             \
		"movw %%ax, %%ds\n\t"                                   \
		"movw %%ax, %%es\n\t"                                   \
		"movw %%ax, %%fs\n\t"                                   \
		"movw %%ax, %%gs"                                       \
		:::"ax")

#define sti() __asm__("sti"::)            // 开中断
#define cli() __asm__("cli"::)            // 关中断
#define nop() __asm__("nop"::)        
#define iret() __asm__("iret"::)

/** \brief 设置门描述符的宏, 需要了解一下门描述符才行
* @param [in] gate_addr 描述符的地址
* @param [in] type 门描述符的类型
* @param [in] dpl 特权层值
* @param [in] addr 偏移地址
*/
#define _set_gate(gate_addr, type, dpl, addr)                   \
__asm__("movw %%dx, %%ax\n\t"                                   \
		"movw %0, %%dx\n\t"                                     \
		"movw %%eax, %1\n\t"                                    \
		"movw %%edx, %2"                                        \
		:: "i" ((short)(0x8000 + (dpl << 13) + (type<<8))),     \
		"o" (*((char*)*(gate_addr))),                           \
		"o" (*(4+(char*)(gate_addr))),                          \
		"d" ((char*)(addr)),                                    \
		"a" (0x00080000))

// 设置中断门, 14为中断门的类型， 0为特权级的值。
#define set_intr_gate(n, addr) _set_gate(&idt[n], 14, 0, addr)
// 设置陷阱门
#define set_trap_gate(n, addr) _set_gate(&idt[n], 15, 0, addr)
// 设置系统调用门
#define set_system_gate(n, addr) _set_gate(&idt[n], 15, 3, addr)

/** \brief 设置段描述符
* @param [in] addr 描述符的地址
* @param [in] type 描述符的类型域值
* @param [in] dpl 特权值
* @param [in] base 段基地址
* @param [in] limit 段限长
*/
#define _set_seg_desc(addr, type, dpl, base, limit) {          \
	*(addr) = ((base) & 0xff000000) |                          \
	          (((base) & 0x00ff0000) >> 16) |                  \
			  ((limit) & 0xf0000) |                            \
			  ((dpl) << 13) |                                  \
			  (0x00408000) |                                   \
			  ((type) << 8);                                   \
	*((addr) + 1) = (((base) & 0x0000ffff) << 16) |            \
	                ((limit) & 0x0ffff);                       \
}


/** \brief 在GDT中设置tss和ldt.
* @param [in] n 要设置的描述符在gdt中的地址
* @param [in] addr tss或ldt的基地址
* @param [in] type 类型
 */
#define _set_tssldt_desc(n, addr, type)                       \
__asm__("movw $104, %1\n\t"                                   \
		::"a" (addr), "m" (*(n)), "m"(*(n+2)),                \
		  "m" (*(n+4)), "m" (*(n+5)), "m" (*(n+6)),           \
		  "m" (*(n+7)))
