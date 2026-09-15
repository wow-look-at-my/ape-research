#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach-o/dyld_images.h>
int main(void) {
	struct dyld_all_image_infos *aii = _dyld_get_all_image_infos();
	printf("_dyld_get_all_image_infos() = %p\n", (void*)aii);
	unsigned char *cb = (unsigned char *)0x0000000FFFFFC000ULL;
	int found = 0;
	for (long off = 0x5800; off <= 0x6800; off += 8) {
		uintptr_t v; memcpy(&v, cb + off, 8);
		if (v == (uintptr_t)aii) { printf("*** MATCH at comm offset 0x%lx\n", off); found=1; }
	}
	if (!found) printf("no match in 0x5800..0x6800\n");
	printf("version=%u infoArray=%p count=%u\n", aii->version, (void*)aii->infoArray, aii->infoArrayCount);
	return 0;
}
