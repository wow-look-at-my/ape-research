#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	unsigned char*b=(unsigned char*)cache;
	printf("cache base = %p\n",(void*)cache);
	printf("magic = %.16s\n",(char*)b);
	/* Dump 64-bit words from 0x00 to 0x200, flag ones that look like pointers/sizes */
	for(int off=0; off<0x200; off+=8){
		uint64_t v=r64(b+off);
		const char*tag="";
		if(v==0x1ff05c000ULL) tag="  <== equals __LINKEDIT vmaddr!";
		else if(v>0x100000000ULL && v<0x10000000000ULL) tag="  <== looks like an address";
		if(v) printf("  +0x%03x = 0x%016llx %-20llu%s\n", off, (unsigned long long)v, (unsigned long long)v, tag);
	}
	return 0;
}
