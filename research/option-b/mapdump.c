#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
int main(void){
  uint64_t base=0x188830000ULL;
  unsigned char*b=(unsigned char*)base;
  printf("raw qwords from 0x228:\n");
  for(int i=0;i<64;i++){
    uint64_t v=*(uint64_t*)(b+0x228+i*8);
    printf("  +%#04x = %#018llx\n",0x228+i*8,(unsigned long long)v);
  }
  return 0;
}
