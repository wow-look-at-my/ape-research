#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
int main(int c,char**v){
  // args: addr1 addr2 ...
  for(int i=1;i<c;i++){
    uint64_t a=strtoull(v[i],0,16);
    printf("== 0x%llx:\n",(unsigned long long)a);
    uint32_t *p=(uint32_t*)a;
    for(int k=0;k<12;k++) printf("  +%02x: %08x\n", k*4, p[k]);
  }
  return 0;
}
