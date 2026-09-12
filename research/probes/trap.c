#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach/mach_time.h>

/* Mach traps on arm64 are class 1: 0x1000000 | number */
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

int main(void) {
	/* sanity: mach_absolute_time is trap 3 */
	unsigned long long t1 = (unsigned long long)mtrap(3,0,0,0,0);
	unsigned long long t2 = mach_absolute_time();
	printf("mach_absolute_time trap=%llu libcall=%llu %s\n", t1, t2, t1 && t1 <= t2*4 ? "(plausible)" : "(SUSPECT)");

	/* mach_task_self_trap = 28 */
	long self = mtrap(28,0,0,0,0);
	printf("mach_task_self_trap = 0x%lx (libSystem says 0x%x)\n", self, mach_task_self());

	/* task_info = 44, TASK_DYLD_INFO = 17 */
	struct { uint64_t addr, size, fmt; } tdi = {0,0,0};
	long kr = mtrap(44, self, 17, (long)&tdi, 3);
	printf("task_info kr=%ld addr=%p size=%llu fmt=%llu\n", kr, (void*)tdi.addr, tdi.size, tdi.fmt);
	if (tdi.addr) {
		uint32_t ver, n;
		__builtin_memcpy(&ver, (void*)tdi.addr, 4);
		__builtin_memcpy(&n, (void*)(tdi.addr+4), 4);
		printf("  version=%u infoArrayCount=%u\n", ver, n);
	}
	return 0;
}
