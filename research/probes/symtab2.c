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
	uint32_t ncmds=r32(h+16);unsigned char*cmd=h+32;
	uint32_t symoff=0,nsyms=0,stroff=0,strsize=0;uint64_t textvm=0,leditvm=0,leditfo=0;
	for(uint32_t i=0;i<ncmds;i++){uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19){char nm[17];memcpy(nm,cmd+8,16);nm[16]=0;
			if(seq(nm,"__TEXT"))textvm=r64(cmd+24);
			if(seq(nm,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}}
		if(c==0x2){symoff=r32(cmd+8);nsyms=r32(cmd+12);stroff=r32(cmd+16);strsize=r32(cmd+20);}
		if(cs<8)break;cmd+=cs;}
	int64_t slide=(int64_t)img-(int64_t)textvm;
	printf("cache=%p img=%p truth=%p\n",(void*)cache,(void*)img,truth);
	printf("symoff=%u nsyms=%u stroff=%u strsize=%u leditvm=%llx leditfo=%llx slide=%lld\n",
	  symoff,nsyms,stroff,strsize,(unsigned long long)leditvm,(unsigned long long)leditfo,(long long)slide);
	/* The shared cache stores symtab/strtab in __LINKEDIT, whose fileoff maps
	   to leditvm. delta = (leditvm+slide) - leditfo */
	int64_t delta=(int64_t)(leditvm+slide)-(int64_t)leditfo;
	unsigned char*st=(unsigned char*)((int64_t)stroff+delta);
	unsigned char*sm=(unsigned char*)((int64_t)symoff+delta);
	printf("delta=%lld  strtab=%p  symtab=%p\n",(long long)delta,(void*)st,(void*)sm);
	printf("first string bytes: ");
	for(int i=0;i<24;i++){unsigned char c=st[i];printf("%c",(c>=32&&c<127)?c:'.');}
	printf("\n");
	int found=0;
	for(uint32_t i=0;i<nsyms && i<400000;i++){
		unsigned char*e=sm+i*16;
		uint32_t strx=r32(e);
		if(strx>=strsize) continue;
		const char*nm=(const char*)(st+strx);
		if(seq(nm,"_getpid")){
			uint64_t val=r64(e+8);
			uint64_t mem=(uint64_t)((int64_t)val+slide);
			printf("FOUND _getpid n_value=%p -> mem=%p truth=%p %s\n",(void*)val,(void*)mem,truth,(mem==(uint64_t)truth)?"MATCH":"differ");
			found=1;break;
		}
	}
	if(!found) printf("_getpid not in symtab (nsyms=%u)\n",nsyms);
	return 0;
}
