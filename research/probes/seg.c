#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
int main(void){
	void*sym=dlsym(RTLD_DEFAULT,"getpid"); Dl_info di; dladdr(sym,&di);
	unsigned char*hd=(unsigned char*)di.dli_fbase;
	printf("base=%p\n",(void*)di.dli_fbase);
	printf("magic=%08x ncmds=%u sizeofcmds=%u\n",r32(hd),r32(hd+16),r32(hd+20));
	uint32_t ncmds=r32(hd+16); unsigned char*cmd=hd+32;
	for(uint32_t i=0;i<ncmds&&i<10;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		if(c==0x19){
			/* segment_command_64: cmd(4) cmdsize(4) segname(16) vmaddr(8)@24 vmsize(8)@32 fileoff(8)@40 filesize(8)@48 */
			char nm[17]; memcpy(nm,cmd+8,16); nm[16]=0;
			printf("  SEG %-16s cmd=%u vmaddr=%llx vmsize=%llx fileoff=%llx filesize=%llx nsects=%u\n",
			 nm,cs,(unsigned long long)r64(cmd+24),(unsigned long long)r64(cmd+32),
			 (unsigned long long)r64(cmd+40),(unsigned long long)r64(cmd+48),r32(cmd+64));
		} else printf("  cmd=0x%x size=%u\n",c,cs);
		if(cs<8)break; cmd+=cs;
	}
	return 0;
}
