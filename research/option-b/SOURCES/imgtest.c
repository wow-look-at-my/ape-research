#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  uint64_t base=0x188830000ULL;
  unsigned char*b=(unsigned char*)base;
  uint64_t ito=*(uint64_t*)(b+0x88), itc=*(uint64_t*)(b+0x90);
  printf("imagesTextOffset=%#llx imagesTextCount=%llu (raw +0x90=%#llx)\n",
    (unsigned long long)ito,(unsigned long long)itc,(unsigned long long)*(uint64_t*)(b+0x90));
  // entry = {uuid[16], loadAddress u64, textSegmentSize u32, pathOffset u32}
  for(int i=0;i<6 && i<(int)itc;i++){
    unsigned char*e=b+ito+i*32;
    uint64_t addr=*(uint64_t*)(e+16);
    uint32_t tss=*(uint32_t*)(e+24);
    uint32_t po=*(uint32_t*)(e+28);
    printf("  [%d] addr=%#llx textSize=%#x pathOff=%#x path=",i,(unsigned long long)addr,tss,po);
    if(po && po<0x8000000 && mapped(base+po)) printf("%.70s\n",(char*)(base+po)); else printf("(bad)\n");
  }
  return 0;
}
