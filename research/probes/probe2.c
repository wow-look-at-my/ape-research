#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <mach-o/dyld.h>
#include <dlfcn.h>
int main(void) {
	int n = _dyld_image_count();
	printf("_dyld_image_count=%d\n", n);
	for (int i = 0; i < n; i++) {
		printf("  [%d] %p %s\n", i, (void*)_dyld_get_image_header(i), _dyld_get_image_name(i));
	}
	printf("RTLD_DEFAULT=%p\n", (void*)RTLD_DEFAULT);
	printf("dlsym default 'getpid' = %p\n", dlsym(RTLD_DEFAULT, "getpid"));
	void *h = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
	printf("dlopen libSystem = %p\n", h);
	printf("dlsym(getpid) = %p\n", h ? dlsym(h, "getpid") : (void*)0);
	return 0;
}
