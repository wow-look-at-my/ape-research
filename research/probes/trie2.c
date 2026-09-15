#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static uint64_t find(const unsigned char*trie,const char*name){
	const unsigned char*p=trie;
	for(;;){
		uint64_t tsize=0;int sh=0;
		while(*p&0x80){tsize|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
		tsize|=(uint64_t)*p<<sh;p++;
		if(tsize){
			if(!*name){
				uint64_t flags=0;sh=0;
				while(*p&0x80){flags|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
				flags|=(uint64_t)*p<<sh;p++;
				uint64_t a=0;sh=0;
				while(*p&0x80){a|=(uint64_t)(*p&0x7f)<<sh;sh+=7;p++;}
				a|=(uint64_t)*p<<sh;
				return (flags&3)?~0ULL:a;
			}
			p+=tsize;
		}
		unsigned nchild=*p++;
		if(!nchild) return 0;
		for(unsigned i=0;i<nchild;i++){
			const char*e=(const char*)p;uint64_t el=0;while(e[el])el++;
			const unsigned char*q=p+el+1;uint64_t off=0;sh=0;
			while(*q&0x80){off|=(uint64_t)(*q&0x7f)<<sh;sh+=7;q++;}
			off|=(uint64_t)*q<<sh;
			if(seq(e,name)){name+=el;p=trie+off;goto next;}
			p=q+1;
		}
		return 0;
	next:;
	}
}
/* Resolve name in the image containing `sym` (found via dladdr), using the
   export trie addressed correctly through the __LINKEDIT slide. */
static uint64_t resolve_in_image(uint64_t imgbase,const char*name,int verbose){
	unsigned char*h=(unsigned char*)imgbase;
	uint64_t textvm=0,leditvm=0,leditfo=0;int have_t=0,have_l=0;
	uint32_t exoff=0;int have_ex=0;
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19){
			char nm[17];memcpy(nm,cmd+8,16);nm[16]=0;
			if(seq(nm,"__TEXT")){textvm=r64(cmd+24);have_t=1;}
			if(seq(nm,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);have_l=1;}
		}
		if(c==0x80000033){exoff=r32(cmd+8);have_ex=1;}
		if(cs<8)break;cmd+=cs;
	}
	if(!have_l||!have_ex){if(verbose)printf("   missing segments/trie\n");return 0;}
	/* slide = where the image actually is minus its unslid __TEXT vmaddr */
	uint64_t slide=imgbase-textvm;
	uint64_t trie=(leditvm+slide)+(exoff-leditfo);
	if(verbose)printf("   slideslide=%llx textvm=%llx leditvm=%llx leditfo=%llx exoff=%u trie=%llx\n",
	  (unsigned long long)slide,(unsigned long long)textvm,(unsigned long long)leditvm,
	  (unsigned long long)leditfo,exoff,(unsigned long long)trie);
	uint64_t r=find((unsigned char*)trie,name);
	if(!r||r==~0ULL)return 0;
	return imgbase+r;   /* trie values are relative to the image's __TEXT base */
}
int main(int argc,char**argv){
	const char*names[]={"getpid","pthread_create","mmap","getentropy","sysctl","dlsym","sysctlbyname","malloc"};
	int n=argc>1?1:8;
	const char*only=argc>1?argv[1]:0;
	/* Use a target image we can cross-check: libsystem_c */
	void*sym=dlsym(RTLD_DEFAULT,"getpid");Dl_info di;dladdr(sym,&di);
	uint64_t base=(uint64_t)di.dli_fbase;
	printf("image: %s at %p\n",di.dli_fname,(void*)base);
	for(int i=0;i<n;i++){
		const char*nm=only?only:names[i];
		uint64_t got=resolve_in_image(base,nm,i==0&&!only);
		void*truth=dlsym(RTLD_DEFAULT,nm);
		printf("  %-16s trie=%p dlsym=%p %s\n",nm,(void*)got,truth,
		  (got&&(void*)got==truth)?"MATCH":(got?"present-but-differs":(truth?"MISSED":"n/a")));
	}
	return 0;
}
