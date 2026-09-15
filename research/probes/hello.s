.globl _start
_start:
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4			// write = 0x2000004
	mov	x0, #1				// stdout
	adr	x1, msg
	mov	x2, #6
	svc	#0x80
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1			// exit = 0x2000001
	mov	x0, #0
	svc	#0x80
msg:
	.ascii	"hello\n"
