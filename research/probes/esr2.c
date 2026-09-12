#include <stdio.h>
#include <errno.h>
static long raw(long n,long a,long b,long c){
  register long x0 __asm__("x0")=a; register long x1 __asm__("x1")=b;
  register long x2 __asm__("x2")=c; register long x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):
    "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0; }
int main(void){ long r=raw(0x2000123,0,0,0); printf("bogus syscall returned %ld (errno %d = %s)\n", r, errno, strerror(errno)); return 0; }
