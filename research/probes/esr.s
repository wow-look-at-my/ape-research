.globl _start
_start:
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4
	mov	x0, #1
	adr	x1, m
	mov	x2, #6
	svc	#0x80
	// deliberately bogus syscall number
	movz	x16, #0x200, lsl #16
	add	x16, x16, #0x123
	mov	x0, #0
	svc	#0x80
	// if we get here, the bogus call returned instead of trapping
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4
	mov	x0, #1
	adr	x1, m2
	mov	x2, #28
	svc	#0x80
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #0
	svc	#0x80
m:	.ascii	"start\n"
m2:	.ascii	"bogus syscall RETURNED\n\n"
