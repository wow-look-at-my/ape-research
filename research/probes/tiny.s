.globl _start
_start:
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #9
	svc	#0x80
