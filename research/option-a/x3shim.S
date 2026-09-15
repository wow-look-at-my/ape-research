// x3shim.S: save the raw x3 (apple vector) at LC_MAIN entry, then call C.
.text
.globl _main
.p2align 2
_main:
    adrp x9, _saved_x3@PAGE
    add  x9, x9, _saved_x3@PAGEOFF
    str  x3, [x9]
    adrp x9, _saved_x0@PAGE
    add  x9, x9, _saved_x0@PAGEOFF
    str  x0, [x9]
    b    _c_main
.data
.globl _saved_x3
_saved_x3: .quad 0
.globl _saved_x0
_saved_x0: .quad 0
