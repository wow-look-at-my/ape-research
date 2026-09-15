/* Find libSystem at runtime WITHOUT linking/importing anything from it. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define COMM_BASE 0x0000000FFFFFC000ULL

int main(void) {
	unsigned char *cb = (unsigned char *)COMM_BASE;
	printf("comm page signature: '%c%c%c%c'\n", cb[0], cb[1], cb[2], cb[3]);
	for (long off = 0x5F00; off < 0x6200; off += 8) {
		uintptr_t v; memcpy(&v, cb + off, 8);
		if (v < 0x100000000ULL || v > 0x0000FFFF00000000ULL) continue;
		uint32_t ver, cnt; memcpy(&ver, (void*)v, 4); memcpy(&cnt, (void*)(v+4), 4);
		if (ver < 13 || ver > 40 || cnt < 1 || cnt > 3000) continue;
		uintptr_t arr; memcpy(&arr, (void*)(v+8), 8);
		if (arr < 0x100000000ULL) continue;
		uintptr_t img0; memcpy(&img0, (void*)arr, 8);
		uintptr_t p0; memcpy(&p0, (void*)(arr+8), 8);
		if (img0 < 0x100000000ULL || p0 < 0x100000000ULL) continue;
		char *path = (char *)p0;
		printf("off=0x%lx aii=%p ver=%u count=%u first_image_path=%.40s\n",
		       off, (void*)v, ver, cnt, path);
	}
	return 0;
}
