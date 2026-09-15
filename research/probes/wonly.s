.globl _main
_main:
	mov	x0, #3
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	svc	#0x80
