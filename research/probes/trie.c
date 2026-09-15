#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}

/* Walk trie and report; count nodes visited so we can detect a malformed walk. */
static uint64_t visited;
static uint64_t find(const unsigned char*trie,const char*name,int depth){
	const unsigned char*p=trie;
	for(;;){
		if(++visited>100000) return 0;
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
int main(int argc,char**argv){
	/* Pick a dylib we can also dlsym for ground truth. */
	const char *which = argc>1?argv[1]:"/usr/lib/system/libsystem_c.dylib";
	void *h=dlopen(which,RTLD_LAZY); if(!h){printf("dlopen fail\n");return 1;}
	/* Find its base via dladdr on a symbol. */
	void *sym=dlsym(h,"getpid"); if(!sym){printf("no getpid\n");return 1;}
	Dl_info di; if(!dladdr(sym,&di)){printf("dladdr fail\n");return 1;}
	uint64_t base=(uint64_t)di.dli_fbase;
	printf("image base=%p  dlsym(getpid)=%p\n",(void*)base,sym);
	unsigned char*hd=(unsigned char*)base;
	uint64_t delta=0;
	uint32_t ncmds=r32(hd+16); unsigned char*cmd=hd+32;
	uint64_t expsz=0;
	for(uint32_t i=0;i<ncmds;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19&&seq((char*)cmd+8,"__LINKEDIT")){
			uint64_t vm=r64(cmd+24); uint64_t fo=r64(cmd+40);
			delta=vm-fo; printf("__LINKEDIT vm=%llx fileoff=%llx delta=%llx\n",(unsigned long long)vm,(unsigned long long)fo,(unsigned long long)delta);
		}
		if(c==0x80000033){ uint64_t o=r32(cmd+8); expsz=r32(cmd+12);
			visited=0;
			uint64_t r=find((unsigned char*)(o+delta),"getpid",0);
			printf("trie dataoff=%llu (delta-adj=%llx) size=%llu visited=%llu\n",(unsigned long long)o,(unsigned long long)(o+delta),(unsigned long long)expsz,(unsigned long long)visited);
			printf("trie getpid -> %s %llx\n", (r&&r!=~0ULL)?"FOUND":"miss", (unsigned long long)r);
			if(r&&r!=~0ULL) printf("  base+off = %p   (dlsym = %p) %s\n",(void*)(base+r),sym,(base+r==(uint64_t)sym)?"MATCH":"DIFFER");
		}
		if(cs<8)break; cmd+=cs;
	}
	return 0;
}
