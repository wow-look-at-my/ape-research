#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
static sigjmp_buf jb;
static void h(int s){ (void)s; siglongjmp(jb,1); }
#define COMM 0xFFFFFC000ULL
int main(void) {
	signal(SIGBUS, h); signal(SIGSEGV, h);
	printf("comm signature: %.16s\n", (char*)COMM);
	for (long off = 0; off < 0x4000; off += 8) {
		if (sigsetjmp(jb,1)) { printf("BUS at 0x%lx\n", off); break; }
		uintptr_t v; memcpy(&v, (void*)(COMM+off), 8);
		if (v > 0x100000000ULL && v < 0x0000FFFF00000000ULL && (v & 3) == 0)
			printf("comm+0x%03lx = 0x%012lx\n", off, (unsigned long)v);
	}
	return 0;
}
