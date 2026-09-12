/* Dump the dyld shared cache header as raw words so we can identify the real
   imagesOffset/imagesCount fields empirically. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
int main(void){
	void *h = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
	/* shared_region_check_np via dlsym-free: we are a normal binary, use syscall */
	typedef long (*f_t)(uint64_t*);
	printf("(using direct syscall)\n");
	uint64_t base=0; long r;
	{
	  register long x0 __asm__("x0")=(long)&base;
	  register long x16 __asm__("x16")=0x2000000|294;
	  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):
	    "x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
	  r=x0;
	}
	printf("ret=%ld base=0x%llx\n", r, (unsigned long long)base);
	if(!base) return 1;
	unsigned char *b=(unsigned char*)base;
	printf("magic: %.16s\n", (char*)b);
	/* header size is at offset 4? print first 32 words */
	for(int i=0;i<32;i++){ uint32_t v; memcpy(&v,b+i*4,4); printf("  +0x%03x (w%-2d) = 0x%08x  %u\n", i*4, i, v, v); }
	return 0;
}
