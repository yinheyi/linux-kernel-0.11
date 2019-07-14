/**
 asm.s汇编程序中包括大部分cpu探测到的异常故障处理的底层代码。该文件主要是通过汇编语言来调用trap.cpp中定义的函数。
 我在这里有一个疑问：为什么不直接调用traps.cpp中的函数，而是通过asm.s文件内的汇编语言来调用呢？
*/

.globl _divide_error, _debug, _nmi, _int3, _overflow, _bounds, _invalid_op
.globl _double_fault, _coprocessor, _segment_overrun
.globl _invalid_TSS, _segment_not_present, stack_segment
.globl _general_protection, _coprocessor_error, _irq13, _reserved

_divide_error:
	pushl $_do_divide_error
no_error_code:
	xchgl %eax, (%esp)
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	push %ds
	push %es
	push %fs
	push $0			// 作为接下来调用的参数
	lea 44(%esp), %edx
	pushl %edx		// 作为接下来调用的参数
	movl $0x10, %edx
	mov %dx, %ds
	mov %dx, %ds
	mov %dx, %fs
	call * %eax
	addl $8, %esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %sdi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

_debug:
	pushl $_do_int3
	jmp no_error_code
_nmi:
	pushl $_do_nmi
	jmp no_error_code
_int3:
	pushl $_do_int3
	jmp no_error_code
_overflow:
	pushl $_do_overflow
	jmp no_error_code
_bounds:
	pushl $_do_bounds
	jmp no_error_code
_invalid_op:
	pushl $_do_invalid_op
	jmp no_error_code
_coprocessor_segment_overrun:
	pushl $_do_coprocessor_segment_overrun
	jmp no_error_code
_reserved:
	pushl $_do_reserved
	jmp no_error_code
_irq13:
	pushl %eax
	xorb %al, %al
	outb %al, $0xF0
	movb $0x20, %al
	outb %al, $0x20
	jmp 1f
1:	jmp 1f
1:	outb %al, $0xA0
	pupl %eax
	jmp _coprocessor_error
_double_fault:
	pushl $_do_double_fault
error_code:
	xchgl %eax, 4(%esp)
	xchgl %ebx, (%esp)
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl ebp
	push %ds
	push %es
	push %fs
	pushl %eax
	lea 44(%esp), %eax
	pushl %eax
	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	call * %ebx
	addl $8, %esp
	pop %fs
	pop %es
	pop %ds
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret

_invalid_TSS:
	pushl $_do_invalid_TSS
	jmp error_code
_segment_not_present:
	pushl $_do_segment_not_present
	jmp error_code
_stack_segment:
	pushl $_do_stack_segment
	jmp error_code
_general_protection:
	pushl $_do_general_protection
	jmp error_code
