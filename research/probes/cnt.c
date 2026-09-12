#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	const unsigned char*h=(const unsigned char*)cache;
	uint32_t ioff=r32(h+0x1c0), icnt=r32(h+0x1c4);
	uint64_t rs=r64(h+0xe0); int64_t slide=(int64_t)cache-(int64_t)rs;
	printf("imagesOffset=%u imagesCount=%u slide=%lld\n",ioff,icnt,(long long)slide);
	const unsigned char*arr=(const unsigned char*)(cache+ioff);
	int valid=0, zero=0, badmagic=0, notdylib=0;
	for(uint32_t i=0;i<icnt;i++){
		const unsigned char*e=arr+(uint64_t)i*32;
		uint64_t au=r64(e); uint32_t pfo=r32(e+24);
		if(!au){zero++;continue;}
		uint64_t a=au+(uint64_t)slide;
		if(r32((const unsigned char*)a)!=0xfeedfacf){badmagic++;continue;}
		uint32_t ft=r32((const unsigned char*)a+12);
		if(ft!=6 && ft!=8) notdylib++;
		valid++;
	}
	printf("entries=%u  valid Mach-O=%d (zero-addr=%d, bad-magic=%d)  non-dylib(filetype!=6,8)=%d\n",
	 icnt,valid,zero,badmagic,notdylib);
	printf("filetype 6=MH_DYLIB 8=MH_BUNDLE\n");
	/* Where the array ends, per the header */
	printf("array spans file offsets %u..%u\n", ioff, ioff+icnt*32);
	return 0;
}
