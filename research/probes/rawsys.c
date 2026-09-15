#include <stdio.h>
/* Raw BSD syscall: class 1 = 0x2000000. Tested from a normally-linked binary. */
static long raw3(long n, long a, long b, long c) {
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
	return x0;
}
int main(void) {
	long w = raw3(0x2000004, 1, (long)"RAW SVC WRITE OK\n", 18);
	printf("raw write returned %ld\n", w);
	long p = raw3(0x2000014, 0, 0, 0);   /* getpid xnu = 20 */
	printf("raw getpid returned %ld\n", p);
	/* raw mmap: class1 #197 xnu? use machdep-free: test munmap-less anon mmap */
	return 0;
}
