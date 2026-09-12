/* How many Syslib entries actually resolve by content? If the payload needs one
   we leave null, it dies silently. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static uint64_t image_symbol(uint64_t img,const char*name){
	const unsigned char*h=(const unsigned char*)img;
	char want[96];{uint64_t i=1;want[0]='_';while(name[i-1]&&i<95){want[i]=name[i-1];i++;}want[i]=0;}
	uint32_t ncmds=r32(h+16); if(ncmds>8192)return 0;
	const unsigned char*cmd=h+32;
	uint64_t textvm=0,leditvm=0,leditfo=0; uint32_t symoff=0,nsyms=0,stroff=0,strsize=0;
	for(uint32_t i=0;i<ncmds;i++){uint32_t c=r32(cmd),cs=r32(cmd+4); if(cs<8)return 0;
		if(c==0x19){const char*sg=(const char*)(cmd+8);
			if(seq(sg,"__TEXT"))textvm=r64(cmd+24);
			else if(seq(sg,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}}
		if(c==0x2){symoff=r32(cmd+8);nsyms=r32(cmd+12);stroff=r32(cmd+16);strsize=r32(cmd+20);}
		cmd+=cs;}
	if(!nsyms||!leditvm||!textvm)return 0;
	int64_t slide=(int64_t)img-(int64_t)textvm;
	int64_t delta=(int64_t)(leditvm+slide)-(int64_t)leditfo;
	const unsigned char*st=(const unsigned char*)((int64_t)stroff+delta);
	const unsigned char*sm=(const unsigned char*)((int64_t)symoff+delta);
	for(uint32_t i=0;i<nsyms;i++){const unsigned char*e=sm+(uint64_t)i*16;
		uint32_t sx=r32(e); if(sx>=strsize)continue;
		unsigned char nt=e[4]; if(nt&0xe0)continue; if((nt&0x0e)!=0x0e)continue;
		if(seq((const char*)(st+sx),want))return r64(e+8)+(uint64_t)slide;}
	return 0;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	const unsigned char*h=(const unsigned char*)cache;
	uint32_t ioff=r32(h+0x1c0), icnt=r32(h+0x1c4);
	int64_t slide=(int64_t)cache-(int64_t)r64(h+0xe0);
	const unsigned char*arr=(const unsigned char*)(cache+ioff);
	const char*names[]={"fork","pipe","clock_gettime","nanosleep","mmap",
	 "pthread_jit_write_protect_supported_np","pthread_jit_write_protect_np",
	 "sys_icache_invalidate","pthread_create","pthread_exit","pthread_kill",
	 "pthread_sigmask","pthread_setname_np","dispatch_semaphore_create",
	 "dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime",
	 "pthread_self","dispatch_release","raise","pthread_join","pthread_yield_np",
	 "pthread_attr_init","pthread_attr_destroy","pthread_attr_setstacksize",
	 "pthread_attr_setguardsize","exit","close","munmap","openat","write","read",
	 "sigaction","pselect","mprotect","sigaltstack","getentropy","sem_open",
	 "sem_unlink","sem_close","sem_post","sem_wait","sem_trywait","getrlimit",
	 "setrlimit","dlopen","dlsym","dlclose","dlerror","pthread_cpu_number_np",
	 "sysctl","sysctlbyname","sysctlnametomib"};
	int miss=0; unsigned total=sizeof(names)/sizeof(names[0]);
	for(unsigned i=0;i<total;i++){
		uint64_t v=0;
		for(uint32_t k=0;k<icnt&&!v;k++){
			const unsigned char*e=arr+(uint64_t)k*32;
			uint64_t au=r64(e); if(!au)continue;
			uint64_t a=au+(uint64_t)slide;
			if(r32((const unsigned char*)a)!=0xfeedfacf)continue;
			if(r32((const unsigned char*)a+12)!=6)continue;
			v=image_symbol(a,names[i]);
		}
		if(!v){printf("  MISSING %s\n",names[i]);miss++;}
	}
	printf("resolved %u/%u (missing %d)\n",total-miss,total,miss);
	return 0;}
