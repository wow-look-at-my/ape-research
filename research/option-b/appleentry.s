.globl _start
.globl __apple_vec
.p2align 2
_start:
    adrp x9, __apple_vec@PAGE
    add  x9, x9, __apple_vec@PAGEOFF
    str  x3, [x9]
    b    _main
.data
.globl __apple_vec
__apple_vec:
    .quad 0
