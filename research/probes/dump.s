.globl _start
_start:
	mov	x19, sp
	ldr	x0, [x19]		// argc
	// print argc as raw write of 1 byte count via svc
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4
	mov	x1, x19
	mov	x2, #64
	mov	x0, #1
	svc	#0x80
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #0
	svc	#0x80
