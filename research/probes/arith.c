/* Derive the trie address formula empirically. In a shared cache the __LINKEDIT
   fileoff is relative to the CACHE START, and the cache is mapped such that
   cacheBase == the first image's __TEXT vmaddr, unslid. So:
       trie_vmaddr = cacheBase_unslid + exoff
   and we reach memory at trie_vmaddr via the slide between unslid and actual. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static uint64_t find(const unsigned char*t,const char*name,int*d){
	const unsigned char*p=t;
	for(;;){ if(++*d>200000)return 0;
		uint64_t ts=0;int sh=0;
		while(*p&0x80){ts|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
		ts|=(uint64_t)*p<<sh;p++;
		if(ts){ if(!*name){uint64_t fl=0;sh=0;
			while(*p&0x80){fl|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
			fl|=(uint64_t)*p<<sh;p++;uint64_t a=0;sh=0;
			while(*p&0x80){a|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
			a|=(uint64_t)*p<<sh;return (fl&3)?~0ULL:a;}
			p+=ts;}
		unsigned nc=*p++;if(!nc)return 0;
		for(unsigned i=0;i<nc;i++){const char*e=(const char*)p;uint64_t el=0;while(e[el])el++;
			const unsigned char*q=p+el+1;uint64_t o=0;sh=0;
			while(*q&0x80){o|=(uint64_t)(*q&0x7f)<<sh;sh+=7;q++;}
			o|=(uint64_t)*q<<sh;
			if(seq(e,name)){name+=el;p=t+o;goto next;} p=q+1;}
		return 0;
	next:;} }
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	void*truth=dlsym(RTLD_DEFAULT,"getpid");Dl_info di;dladdr(truth,&di);
	uint64_t img=(uint64_t)di.dli_fbase;
	unsigned char*h=(unsigned char*)img;
	uint32_t exoff=0;uint64_t textvm=0;
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19&&seq((char*)cmd+8,"__TEXT"))textvm=r64(cmd+24);
		if(c==0x80000033)exoff=r32(cmd+8);
		if(cs<8)break;cmd+=cs;}
	/* The cache's unslid image-0 __TEXT vmaddr is the cache base's unslid addr.
	   The memory slide = actual cache base - unslid cache base. Unslipped cache
	   base is typically 0x180000000 for arm64e caches. */
	printf("cache(mem)=%p img=%p textvm=%llx (slid from unslid)\n",(void*)cache,(void*)img,(unsigned long long)textvm);
	printf("slide = cache - textvm = %lld\n",(long long)((int64_t)cache-(int64_t)textvm));
	/* The image's own unslid vmaddr is textvm, so its actual base is cache
	   (because cache base IS image 0's TEXT; but our img is a later image).
	   Real relation: img_actual = cacheBase_mem + (img_unslid - cacheBase_unslid).
	   We know img_unslid = textvm. cacheBase_unslid = ? Compute via slide:
	   slide = cacheBase_mem - cacheBase_unslid  => cacheBase_unslid = cache - slide
	   but we can get slide from any image: img_mem - img_unslid = slide. */
	int64_t slide=(int64_t)img-(int64_t)textvm;
	printf("slide from img: %lld\n",(long long)slide);
	uint64_t cachebase_unslid=(uint64_t)((int64_t)cache-slide);
	printf("cacheBase_unslid = %p\n",(void*)cachebase_unslid);
	uint64_t trie_unslid=cachebase_unslid+exoff;
	uint64_t trie_mem=(uint64_t)((int64_t)trie_unslid+slide);
	printf("trie_unslid=%p trie_mem=%p\n",(void*)trie_unslid,(void*)trie_mem);
	int d=0; uint64_t r=find((unsigned char*)trie_mem,"getpid",&d);
	printf("find(getpid) -> %llx depth=%d\n",(unsigned long long)r,d);
	if(r&&r!=~0ULL) printf("  img+off=%p truth=%p %s\n",(void*)(img+r),truth,(img+r==(uint64_t)truth)?"MATCH":"DIFFER");
	return 0;
}
