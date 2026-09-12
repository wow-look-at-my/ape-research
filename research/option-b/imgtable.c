#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
int main(void){
  uint64_t base=0x188830000ULL;
  unsigned char*b=(unsigned char*)base;
  // imagesTextOffset at +136, count at +144
  uint64_t ito=*(uint64_t*)(b+136), itc=*(uint64_t*)(b+144);
  printf("imagesTextOffset=%#llx count=%llu\n",(unsigned long long)ito,(unsigned long long)itc);
  printf("raw bytes at ito (%#llx):\n",(unsigned long long)(base+ito));
  for(int i=0;i<8;i++){
    unsigned char*e=b+ito+i*32;
    uint64_t a0=*(uint64_t*)(e+0),a1=*(uint64_t*)(e+8),a2=*(uint64_t*)(e+16);
    uint32_t pfo=*(uint32_t*)(e+24);
    printf("  [%d] q0=%#llx q1=%#llx q2=%#llx pfo=%#x (as path: %.40s)\n",i,
      (unsigned long long)a0,(unsigned long long)a1,(unsigned long long)a2,pfo,
      (pfo>0&&pfo<0x10000000)?(const char*)(base+pfo):"?");
  }
  // Try the OLD images table (array of 32-bit offsets at imagesOffsetOld)
  uint32_t ioo=*(uint32_t*)(b+24), ioc=*(uint32_t*)(b+28);
  printf("imagesOffsetOld=%#x count=%u\n",ioo,ioc);
  if(ioo){ printf("  raw at old:\n"); for(int i=0;i<4;i++){ uint32_t o=*(uint32_t*)(b+ioo+i*4); printf("    [%d] off=%#x path=%.60s\n",i,o,(const char*)(base+o)); } }
  return 0;
}
