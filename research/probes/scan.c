#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/task_info.h>

/* Validate a candidate dyld_all_image_infos without touching libSystem. */
static int valid(uintptr_t p, uintptr_t *out_count) {
	if (p < 0x100000000ULL || p > 0x0000FFFF00000000ULL || (p & 7)) return 0;
	uint32_t ver, n;
	memcpy(&ver, (void*)p, 4);
	memcpy(&n, (void*)(p+4), 4);
	if (ver < 10 || ver > 40 || n < 1 || n > 4000) return 0;
	uintptr_t arr; memcpy(&arr, (void*)(p+8), 8);
	if (arr < 0x100000000ULL || (arr & 7)) return 0;
	uintptr_t img, path;
	memcpy(&img, (void*)arr, 8);
	memcpy(&path, (void*)(arr+8), 8);
	if (img < 0x100000000ULL || path < 0x100000000ULL) return 0;
	*out_count = n;
	return 1;
}

int main(void) {
	unsigned char *cb = (unsigned char *)0x0000000FFFFFC000ULL;
	printf("comm signature: %.16s\n", cb);
	int hits = 0;
	for (long off = 0; off < 0x4000 - 8; off += 8) {
		uintptr_t v; memcpy(&v, cb+off, 8);
		uintptr_t n;
		if (valid(v, &n)) { printf("*** candidate at comm+0x%03lx -> %p count=%lu\n", off, (void*)v, n); hits++; }
	}
	printf("%d candidate(s)\n", hits);
	return 0;
}
