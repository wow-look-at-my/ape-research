/* Find the cache's image-info array. Its entries are {addr,modTime,inode,pathOff,pad}.
   Validate by checking that entry 0's path string looks like a real path under /usr/lib or /System. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}

static int starts(const char*s,const char*p){while(*p){if(*s!=*p)return 0;s++;p++;}return 1;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	const unsigned char*b=(const unsigned char*)cache;
	printf("cache=%p\n",(void*)cache);
	/* Candidate (offset,count) locations seen in dyld_cache_format.h revisions. */
	struct { int oo,co; const char*n; } cand[]={
		{0x18,0x1C,"imagesOffsetOld/imagesCountOld (early)"},
		{0x28,0x2C,"imagesOffset/imagesCount (mid)"},
		{0x150,0x154,"alt 0x150"},
		{0x160,0x164,"alt 0x160"},
	};
	for(unsigned k=0;k<4;k++){
		uint32_t o=r32(b+cand[k].oo),c=r32(b+cand[k].co);
		printf("%s: off=%u count=%u\n",cand[k].n,o,c);
		if(o<0x1000||o>0x100000||c<10||c>20000) continue;
		const unsigned char*arr=b+o;
		uint64_t a0=r64(arr); uint32_t p0=r32(arr+24);
		if(a0<0x180000000ULL||a0>0x300000000ULL) continue;
		if(p0>=0x80000000ULL) continue;
		const char*path=(const char*)(b+p0);
		printf("   entry0 addr=%p pathOff=%u path=\"%.60s\"\n",(void*)a0,p0,path);
	}
	return 0;
}
