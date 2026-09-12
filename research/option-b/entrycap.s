.globl _start
.globl _ent_x30
.globl _ent_sp
.p2align 2
_start:
    adrp x9, _ent_x30@PAGE
    add  x9, x9, _ent_x30@PAGEOFF
    str  x30, [x9]
    mov  x10, sp
    adrp x9, _ent_sp@PAGE
    add  x9, x9, _ent_sp@PAGEOFF
    str  x10, [x9]
    b    _main
.data
.p2align 3
_ent_x30: .quad 0
_ent_sp:  .quad 0
