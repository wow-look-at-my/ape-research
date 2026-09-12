// long rs_syscall6(long classnum, long a, long b, long c, long d, long e, long f)
//   x0 = class-tagged syscall number (0x2000000|n for BSD, 0x1000000|n for mach)
//   x1..x6 = arguments a..f
// Returns x0; error returns -errno (Linux convention) by negating on carry.
.globl _rs_syscall6
.p2align 2
_rs_syscall6:
    mov  x16, x0
    mov  x0, x1
    mov  x1, x2
    mov  x2, x3
    mov  x3, x4
    mov  x4, x5
    mov  x5, x6
    svc  #0x80
    b.cc 1f
    neg  x0, x0
1:  ret
