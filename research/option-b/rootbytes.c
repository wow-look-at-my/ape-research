#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  uint64_t slide=0x8830000ULL, ldvm=0x1ff05c000ULL, ldfo=0x4000ULL;
  uint32_t offs[]={0xb3cce0,0xb686d0,0xb71498,0xb4dc50};
  for(int i=0;i<4;i++){
    uint64_t t=ldvm+slide+((uint64_t)offs[i]-ldfo);
    printf("off=%#x trie=%#llx mapped=%d: ",offs[i],(unsigned long long)t,mapped(t));
    if(mapped(t)){ unsigned char*b=(unsigned char*)t; for(int k=0;k<20;k++)printf("%02x ",b[k]); printf("| ");
      for(int k=0;k<20;k++){char c=b[k];putchar((c>=32&&c<127)?c:'.');} }
    printf("\n"); fflush(stdout);
  }
  return 0;
}
