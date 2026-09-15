/* For the cache image that should hold libsystem_c, dump its LC_SYMTAB and check
   the computed strtab/symtab addresses are readable. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	const unsigned char*h=(const unsigned char*)cache;
	uint32_t ioff=r32(h+0x1c0), icnt=r32(h+0x1c4);
	uint64_t rs=r64(h+0xe0);
	int64_t slide=(int64_t)cache-(int64_t)rs;
	printf("cache=%p regionStart=%p slide=%lld ioff=%u icnt=%u\n",(void*)cache,(void*)rs,(long long)slide,ioff,icnt);
	const unsigned char*arr=(const unsigned char*)(cache+ioff);
	int shown=0;
	for(uint32_t i=0;i<icnt && shown<4;i++){
		const unsigned char*e=arr+(uint64_t)i*32;
		uint64_t au=r64(e); uint32_t pfo=r32(e+24);
		if(!au||!pfo)continue;
		uint64_t addr=au+(uint64_t)slide;
		const char*p=(const char*)(cache+pfo);
		/* Only look at images with a symtab */
		if(r32((const unsigned char*)addr)!=0xfeedfacf) { continue; }
		if(r32((const unsigned char*)addr+12)!=6) continue;
		const unsigned char*hh=(const unsigned char*)addr;
		uint32_t ncmds=r32(hh+16);
		if(ncmds>8192){printf("  [%u] absurd ncmds %u at %p path=%.40s\n",i,ncmds,(void*)addr,p);continue;}
		const unsigned char*cmd=hh+32;
		uint32_t symoff=0,nsyms=0,stroff=0,strsize=0;uint64_t textvm=0,leditvm=0,leditfo=0;
		for(uint32_t k=0;k<ncmds;k++){
			uint32_t c=r32(cmd),cs=r32(cmd+4);
			if(cs<8)break;
			if(c==0x19){const char*sg=(const char*)(cmd+8);
				if(eq(sg,"__TEXT"))textvm=r64(cmd+24);
				else if(eq(sg,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}}
			if(c==0x2){symoff=r32(cmd+8);nsyms=r32(cmd+12);stroff=r32(cmd+16);strsize=r32(cmd+20);}
			cmd+=cs;}
		printf("  [%u] addr=%p textvm=%llx leditvm=%llx leditfo=%llx symoff=%u nsyms=%u stroff=%u strsize=%u\n",
		 i,(void*)addr,(unsigned long long)textvm,(unsigned long long)leditvm,(unsigned long long)leditfo,symoff,nsyms,stroff,strsize);
		printf("      path=%.50s\n",p);
		if(nsyms){
			int64_t sl=(int64_t)addr-(int64_t)textvm;
			int64_t delta=(int64_t)(leditvm+sl)-(int64_t)leditfo;
			printf("      slide_from_img=%lld delta=%lld strtab=%p symtab=%p\n",
			 (long long)sl,(long long)delta,(void*)((int64_t)stroff+delta),(void*)((int64_t)symoff+delta));
			printf("      first strings: %.30s\n",(char*)((int64_t)stroff+delta));
			shown++;
		}
	}
	return 0;
}
