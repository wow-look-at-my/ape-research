.globl _start
_start:
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	mov	x0, #42
	svc	#0x80
.globl _main
_main: b _start
