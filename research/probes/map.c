/* Read the cache mapping table (mappingOffset@0x10, mappingCount@0x14) to learn
   the authoritative file-offset -> vmaddr translation, then resolve a symbol. */
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
	unsigned char*b=(unsigned char*)cache;
	uint32_t mo=r32(b+0x10), mc=r32(b+0x14);
	printf("mappingOffset=%u mappingCount=%u\n",mo,mc);
	for(uint32_t i=0;i<mc && i<8;i++){
		unsigned char*m=b+mo+i*32;
		uint64_t addr=r64(m),size=r64(m+8),fo=r64(m+16);
		printf("  map[%u] addr=%p size=0x%llx fileOffset=%llu maxProt=%u initProt=%u\n",
		  i,(void*)addr,(unsigned long long)size,(unsigned long long)fo,r32(m+24),r32(m+28));
	}
	/* subCacheArrayOffset @ 0x130? print a few late fields that look like arrays */
	for(int off=0xA0; off<0x200; off+=8){
		uint64_t v=r64(b+off);
		if(v>0x180000000ULL && v<0x300000000ULL) printf("  +0x%03x = %p (likely vm addr)\n",off,(void*)v);
	}
	void*truth=dlsym(RTLD_DEFAULT,"getpid");Dl_info di;dladdr(truth,&di);
	uint64_t img=(uint64_t)di.dli_fbase;
	printf("\nimg=%p truth(getpid)=%p\n",(void*)img,truth);
	unsigned char*h=(unsigned char*)img;
	uint32_t exoff=0;uint64_t textvm=0,leditvm=0,leditfo=0;
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19){char nm[17];memcpy(nm,cmd+8,16);nm[16]=0;
			if(seq(nm,"__TEXT"))textvm=r64(cmd+24);
			if(seq(nm,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}}
		if(c==0x80000033)exoff=r32(cmd+8);
		if(cs<8)break;cmd+=cs;}
	int64_t slide=(int64_t)img-(int64_t)textvm;
	printf("textvm=%llx leditvm=%llx leditfo=%llx exoff=%u slide=%lld\n",
	  (unsigned long long)textvm,(unsigned long long)leditvm,(unsigned long long)leditfo,exoff,(long long)slide);
	/* Hypothesis A: trie vmaddr = cachebase_unslid + exoff  (cachebase_unslid=0x180000000) */
	/* Hypothesis B: trie vmaddr = leditvm + (exoff - leditfo) */
	uint64_t a_unslid=0x180000000ULL+exoff;
	uint64_t b_unslid=leditvm+(exoff-leditfo);
	uint64_t cand[2]={a_unslid,b_unslid};
	const char*nm[2]={"A cachebase+exoff","B leditvm+(exoff-leditfo)"};
	for(int k=0;k<2;k++){
		uint64_t mem=(uint64_t)((int64_t)cand[k]+slide);
		int d=0;uint64_t r=find((unsigned char*)mem,"getpid",&d);
		printf("%s -> vm=%p mem=%p find=%llx depth=%d",nm[k],(void*)cand[k],(void*)mem,(unsigned long long)r,d);
		if(r&&r!=~0ULL){ printf("  img+off=%p %s",(void*)(img+r),(img+r==(uint64_t)truth)?"MATCH":"differ"); }
		printf("\n");
	}
	return 0;
}
