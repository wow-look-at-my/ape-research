/* Find where an export trie REALLY lives. We know a symbol's true address from
   dlsym, so we can search memory for the trie node containing its name and
   thereby derive the correct base. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	void*truth=dlsym(RTLD_DEFAULT,"getpid");Dl_info di;dladdr(truth,&di);
	uint64_t img=(uint64_t)di.dli_fbase;
	unsigned char*h=(unsigned char*)img;
	uint32_t exoff=0;uint64_t textvm=0,leditvm=0,leditfo=0;
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19){char nm[17];memcpy(nm,cmd+8,16);nm[16]=0;
			if(seq(nm,"__TEXT"))textvm=r64(cmd+24);
			if(seq(nm,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}}
		if(c==0x80000033)exoff=r32(cmd+8);
		if(cs<8)break;cmd+=cs;
	}
	printf("cache=%p img=%p truth(getpid)=%p\n",(void*)cache,(void*)img,truth);
	printf("textvm=%llx leditvm=%llx leditfo=%llx exoff=%u\n",
	  (unsigned long long)textvm,(unsigned long long)leditvm,(unsigned long long)leditfo,exoff);
	/* The cache base equals the __TEXT vmaddr of the first image, unslid.
	   Compute the cache->memory slide: find any image whose vmaddr we can compare. */
	printf("\nSearch: where does 'getpid' appear as a trie edge near its true addr?\n");
	/* Search a window around the true address for the string 'getpid' */
	uint64_t t=(uint64_t)truth;
	unsigned char*lo=(unsigned char*)(t & ~0xFFFFFULL);
	int hits=0;
	for(long off=0; off<0x1000000; off++){
		if(seq((char*)(lo+off),"getpid")){
			printf("  'getpid' at %p  (truth-here=%lld)\n",(void*)(lo+off),(long long)((uint64_t)(lo+off)-t));
			if(++hits>8)break;
		}
	}
	printf("hits=%d\n",hits);
	printf("\nCompare: does cacheBase+exoff land anywhere sane?\n");
	printf("  cache+exoff = %p\n",(void*)(cache+exoff));
	printf("  img+exoff   = %p\n",(void*)(img+exoff));
	printf("  (truth - (cache+exoff)) = %lld\n",(long long)(t-(cache+exoff)));
	printf("  (truth - (img+exoff))   = %lld\n",(long long)(t-(img+exoff)));
	return 0;
}
