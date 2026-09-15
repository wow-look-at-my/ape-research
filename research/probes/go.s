.globl _start
_start:
	ldr	x19, [sp]			// argc
	add	x20, sp, #8			// argv
	mov	x0, x19
	mov	x1, x20
	bl	_go
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #0
	svc	#0x80
