.globl _main
_main:
	// x0=argc x1=argv x2=envp x3=apple
	mov	x19, x3
	mov	x20, x0
	mov	x21, x1
	mov	x22, x2
	// write the pointer value as raw hex via raw write syscall
	sub	sp, sp, #64
	mov	x9, sp
	mov	w10, #0x30
	strb	w10, [x9]
	mov	w10, #0x78
	strb	w10, [x9, #1]
	mov	x11, #15
	mov	x12, #60
1:
	lsr	x13, x19, x12
	and	x13, x13, #0xf
	cmp	x13, #10
	b.lo	2f
	add	x13, x13, #0x57
	b	3f
2:	add	x13, x13, #0x30
3:	add	x14, x9, x11
	strb	w13, [x14]
	sub	x12, x12, #4
	sub	x11, x11, #1
	cmp	x12, #0
	b.ge	1b
	mov	w10, #10
	strb	w10, [x9, #18]
	mov	x0, #1
	mov	x1, x9
	mov	x2, #19
	movz	x16, #0x200, lsl #16
	add	x16, x16, #4
	svc	#0x80
	mov	x0, #0
	movz	x16, #0x200, lsl #16
	add	x16, x16, #1
	svc	#0x80
