.globl _start
_start:
	movq $0x2000001, %rax
	movq $42, %rdi
	syscall
