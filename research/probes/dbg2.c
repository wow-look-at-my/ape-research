/* Instrument the loader's decision points to find where the boot fails. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static uint64_t r64(const unsigned char*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
typedef struct { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; } Phdr;
int main(int argc, char**argv){
	int fd=open(argv[1],O_RDONLY); if(fd<0){perror("open");return 1;}
	unsigned char eh[64];
	uint64_t off=0x10000;
	for(;;off+=0x10000){
		if(pread(fd,eh,64,off)!=64){printf("no payload\n");return 1;}
		if(r32(eh)==0x464c457f && *(uint16_t*)(eh+18)==183) break;
	}
	printf("payload at 0x%llx\n",(unsigned long long)off);
	printf("  entry=0x%llx phoff=%llu phentsize=%u phnum=%u\n",
	 (unsigned long long)r64(eh+24),(unsigned long long)r64(eh+32),*(uint16_t*)(eh+54),*(uint16_t*)(eh+56));
	Phdr ph[18];
	pread(fd,ph,*(uint16_t*)(eh+56)*sizeof(Phdr), r64(eh+32)+off);
	int n=*(uint16_t*)(eh+56);
	uint64_t lo=~0ULL, hi=0;
	for(int i=0;i<n;i++){
		if(ph[i].type!=1||!ph[i].memsz) continue;
		printf("  LOAD[%d] vaddr=0x%llx offset=0x%llx filesz=0x%llx memsz=0x%llx flags=%u (r%c w%c x%c)\n",
		 i,(unsigned long long)ph[i].vaddr,(unsigned long long)ph[i].offset,
		 (unsigned long long)ph[i].filesz,(unsigned long long)ph[i].memsz,ph[i].flags,
		 (ph[i].flags&4)?'r':'-',(ph[i].flags&2)?'w':'-',(ph[i].flags&1)?'x':'-');
		if(ph[i].vaddr<lo)lo=ph[i].vaddr;
		if(ph[i].vaddr+ph[i].memsz>hi)hi=ph[i].vaddr+ph[i].memsz;
	}
	printf("  load range: 0x%llx..0x%llx\n",(unsigned long long)lo,(unsigned long long)hi);
	printf("  page-align check: vaddr&0x3fff vs offset&0x3fff per segment:\n");
	for(int i=0;i<n;i++){
		if(ph[i].type!=1||!ph[i].memsz) continue;
		printf("    [%d] vaddr%%16k=0x%llx offset%%16k=0x%llx %s\n",i,
		 (unsigned long long)(ph[i].vaddr&0x3fff),(unsigned long long)(ph[i].offset&0x3fff),
		 ((ph[i].vaddr&0x3fff)==(ph[i].offset&0x3fff))?"ok":"MISALIGNED");
	}
	/* is the target range free? */
	uint64_t base=lo&~(0x4000ULL-1);
	void*p=mmap((void*)base,hi-base,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0);
	printf("  test mmap at 0x%llx span 0x%llx -> %p %s\n",(unsigned long long)base,
	 (unsigned long long)(hi-base),p, p==(void*)base?"OK":"FAILED");
	(void)argc;
	return 0;
}
