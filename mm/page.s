/** \fl page.s
*
*/

.globl _page_fault

_page_fault:
	xchgl %eax, (%esp)
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs

	movl $0x10, %edx
	mov %dx, %ds
	mov %dx, %es
	mov %dx, %fs
	mov %cr2, %edx

	pushl %edx
	pushl %eax
	test $1, %eax
	jne 1f
	call _do_no_page
	jmp 2f
1:  call _do_wp_page
2:	addl $8, %esp

	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
