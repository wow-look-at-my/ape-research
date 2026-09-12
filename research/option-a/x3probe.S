// x3probe.S: read x3 at LC_MAIN entry, before any C code touches it.
// Writes "x3=<hex> x0=<argc hex>\n" via raw write(2).
.text
.globl _main
.p2align 2
_main:
    // save x0..x3
    mov x19, x0
    mov x20, x3
    // build buffer on stack
    sub sp, sp, #128
    mov x0, sp
    // "x3="
    mov w1, #0x7838        // 'x','8' little endian -> bytes 0x38='8', 0x78='x'
    // we'll write bytes manually
    mov w2, #'x'
    strb w2, [x0]
    mov w2, #'3'
    strb w2, [x0, #1]
    mov w2, #'='
    strb w2, [x0, #2]
    // hex digits of x20 (16)
    mov x3, x20
    mov x4, #0
1:  lsr x5, x3, #60
    and x5, x5, #0xf
    cmp x5, #10
    b.lo 2f
    add x5, x5, #('a'-10)
    b 3f
2:  add x5, x5, #'0'
3:  add x6, x0, #3
    add x6, x6, x4
    strb w5, [x6]
    lsl x3, x3, #4
    add x4, x4, #1
    cmp x4, #16
    b.lo 1b
    mov w2, #' '
    strb w2, [x0, #19]
    mov w2, #'x'
    strb w2, [x0, #20]
    mov w2, #'0'
    strb w2, [x0, #21]
    mov w2, #'='
    strb w2, [x0, #22]
    mov x3, x19
    mov x4, #0
4:  lsr x5, x3, #60
    and x5, x5, #0xf
    cmp x5, #10
    b.lo 5f
    add x5, x5, #('a'-10)
    b 6f
5:  add x5, x5, #'0'
6:  add x6, x0, #23
    add x6, x6, x4
    strb w5, [x6]
    lsl x3, x3, #4
    add x4, x4, #1
    cmp x4, #16
    b.lo 4b
    mov w2, #'\n'
    strb w2, [x0, #39]
    // write(1, sp, 40)
    mov x1, x0
    mov x0, #1
    mov x2, #40
    mov x16, #4
    orr x16, x16, #0x2000000
    svc #0x80
    // exit(0)
    mov x0, #0
    mov x16, #1
    orr x16, x16, #0x2000000
    svc #0x80
    brk #1
