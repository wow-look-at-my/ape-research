#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
/* usage: mt <trapnum> [a b c d] ; prints result or dies */
static long mtrap(long n, long a, long b, long c, long d) {
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x16 __asm__("x16") = 0x1000000 | n;
	__asm__ volatile("svc #0x80" : "+r"(x0)
	    : "r"(x1), "r"(x2), "r"(x3), "r"(x16)
	    : "x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
	return x0;
}
int main(int argc, char **argv) {
	long t = atol(argv[1]);
	long r = mtrap(t, 0, 0, 0, 0);
	printf("trap %ld -> %ld (0x%lx)\n", t, r, (unsigned long)r);
	return 0;
}
