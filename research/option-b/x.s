.globl myvar
myvar: .quad 0
.globl _getmv
_getmv: adrp x0, myvar@PAGE
        add x0, x0, myvar@PAGEOFF
        ret
