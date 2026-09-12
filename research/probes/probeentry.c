/* Verify the payload's first instructions land in memory before we jump. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
typedef struct { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; } Phdr;
int main(int argc,char**argv){
	int fd=open(argv[1],O_RDONLY);
	unsigned char eh[64]; uint64_t off=0x10000;
	for(;;off+=0x10000){ if(pread(fd,eh,64,off)!=64)return 1;
		if(r32(eh)==0x464c457f && *(uint16_t*)(eh+18)==183)break; }
	uint64_t entry=r64(eh+24), phoff=r64(eh+32);
	uint16_t phnum=*(uint16_t*)(eh+56);
	Phdr ph[18]; pread(fd,ph,phnum*sizeof(Phdr),phoff+off);
	for(int i=0;i<phnum;i++){
		if(ph[i].type!=1||!ph[i].memsz)continue;
		uint64_t base=ph[i].vaddr&~(0x4000ULL-1);
		uint64_t span=(ph[i].vaddr+ph[i].memsz)-base;
		int prot=((ph[i].flags&4)?1:0)|((ph[i].flags&2)?2:0)|((ph[i].flags&1)?4:0);
		void*m=mmap((void*)base,span,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0);
		if(m!=(void*)base){printf("seg[%d] mmap FAILED\n",i);continue;}
		if(ph[i].filesz) pread(fd,(void*)ph[i].vaddr,ph[i].filesz,ph[i].offset);
		if(prot!=3) mprotect((void*)base,span,prot);
		printf("seg[%d] vaddr=0x%llx memsz=0x%llx prot=%d ok\n",i,
		 (unsigned long long)ph[i].vaddr,(unsigned long long)ph[i].memsz,prot);
	}
	printf("entry=0x%llx first 16 bytes: ",(unsigned long long)entry);
	for(int i=0;i<16;i++) printf("%02x",((unsigned char*)entry)[i]);
	printf("\n");
	printf("(if these are the ELF's real first instructions, mapping is correct)\n");
	(void)argc; return 0;
}
