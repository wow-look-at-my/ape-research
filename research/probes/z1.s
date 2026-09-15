.globl _start
_start:
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4
	mov	x0, #1
	adr	x1, msg
	mov	x2, #22
	svc	#0x80
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #7
	svc	#0x80
msg:	.ascii	"z1 raw svc, no imports\n"
