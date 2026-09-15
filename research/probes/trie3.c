/* Understand shared-cache addressing. In the cache, dataoff in LC_DYLD_EXPORTS_TRIE
   is an offset from the CACHE BASE (not the image). Verify by resolving a symbol
   we can cross-check with dlsym, then confirm the mapping. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static uint64_t find(const unsigned char*t,const char*name,int*depth){
	const unsigned char*p=t;
	for(;;){
		if(++*depth>200000)return 0;
		uint64_t ts=0;int sh=0;
		while(*p&0x80){ts|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
		ts|=(uint64_t)*p<<sh;p++;
		if(ts){ if(!*name){ uint64_t fl=0;sh=0;
				while(*p&0x80){fl|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
				fl|=(uint64_t)*p<<sh;p++;
				uint64_t a=0;sh=0;
				while(*p&0x80){a|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
				a|=(uint64_t)*p<<sh; return (fl&3)?~0ULL:a; }
			p+=ts; }
		unsigned nc=*p++; if(!nc)return 0;
		for(unsigned i=0;i<nc;i++){
			const char*e=(const char*)p;uint64_t el=0;while(e[el])el++;
			const unsigned char*q=p+el+1;uint64_t o=0;sh=0;
			while(*q&0x80){o|=(uint64_t)(*q&0x7f)<<sh;sh+=7;q++;}
			o|=(uint64_t)*q<<sh;
			if(seq(e,name)){name+=el;p=t+o;goto next;}
			p=q+1; }
		return 0;
	next:; }
}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	printf("cache base = %p\n",(void*)cache);
	/* Ground truth target: a symbol in libsystem_c */
	void*truth=dlsym(RTLD_DEFAULT,"getpid");Dl_info di;dladdr(truth,&di);
	uint64_t img=(uint64_t)di.dli_fbase;
	printf("libsystem_c image = %p   dlsym(getpid)=%p\n",(void*)img,truth);
	unsigned char*h=(unsigned char*)img;
	uint64_t textvm=0;uint32_t exoff=0;int have=0;
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19&&seq((char*)cmd+8,"__TEXT")) textvm=r64(cmd+24);
		if(c==0x80000033){exoff=r32(cmd+8);have=1;}
		if(cs<8)break;cmd+=cs;
	}
	printf("__TEXT vmaddr=%llx  exports dataoff=%u\n",(unsigned long long)textvm,exoff);
	/* Hypothesis: trie address = cacheBase + dataoff */
	int d=0;
	uint64_t base_try = cache + (uint64_t)exoff;
	printf("trying trie = cacheBase+dataoff = %p\n",(void*)base_try);
	uint64_t r=find((unsigned char*)base_try,"getpid",&d);
	printf("  find(getpid) -> %llx  (depth %d)\n",(unsigned long long)r,d);
	if(r&&r!=~0ULL){
		printf("  img+off = %p   truth = %p   %s\n",(void*)(img+r),truth,(img+r==(uint64_t)truth)?"MATCH":"DIFFER");
	}
	return 0;
}
