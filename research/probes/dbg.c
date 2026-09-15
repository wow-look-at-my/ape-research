#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
int main(void){
	uint64_t base=0;
	{register long x0 __asm__("x0")=(long)&base;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	unsigned char *b=(unsigned char*)base;
	/* libsystem_c.dylib was at 0x188bb0000 in the scan */
	uint64_t addr = 0x188bb0000ULL;
	unsigned char *h=(unsigned char*)addr;
	printf("magic=%08x cputype=%08x filetype=%u ncmds=%u sizeofcmds=%u\n",
	       r32(h), r32(h+4), r32(h+12), r32(h+16), r32(h+20));
	uint32_t ncmds=r32(h+16);
	unsigned char *cmd=h+32;
	for(uint32_t i=0;i<ncmds;i++){
		uint32_t c=r32(cmd),cs=r32(cmd+4);
		const char*nm="?";
		if(c==0x19)nm="LC_SEGMENT_64"; else if(c==0x2)nm="LC_SYMTAB";
		else if(c==0xb)nm="LC_DYSYMTAB"; else if(c==0xd)nm="LC_ID_DYLIB";
		else if(c==0x22)nm="LC_DYLD_INFO"; else if(c==0x80000022)nm="LC_DYLD_INFO_ONLY";
		else if(c==0x80000033)nm="LC_DYLD_EXPORTS_TRIE"; else if(c==0x80000034)nm="LC_DYLD_EXPORTS_TRIE?";
		else if(c==0x80000035)nm="LC_DYLD_CHAINED_FIXUPS";
		printf("  [%u] %-22s cmd=0x%-10x size=%u", i, nm, c, cs);
		if(c==0x80000033) printf("  dataoff=%u datasize=%u", r32(cmd+8), r32(cmd+12));
		if(c==0x80000022||c==0x22) printf("  export_off=%u export_size=%u", r32(cmd+40), r32(cmd+44));
		if(c==0x19) printf("  segname=%.16s fileoff=%llu", (char*)cmd+8, (unsigned long long)((uint64_t)r32(cmd+64)|((uint64_t)r32(cmd+68)<<32)));
		printf("\n");
		if(cs<8) break;
		cmd+=cs;
	}
	return 0;
}
