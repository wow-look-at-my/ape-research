#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  uint64_t lh=0x18890f000ULL;
  uint64_t lt=lh+0xb3cce0ULL;
  printf("lt=%#llx mapped=%d\n",(unsigned long long)lt,mapped(lt));
  fflush(stdout);
  if(mapped(lt)){
    uint8_t*b=(uint8_t*)lt;
    printf("bytes:"); for(int i=0;i<32;i++) printf(" %02x",b[i]); printf("\n");
    printf("ascii:"); for(int i=0;i<32;i++){ char c=b[i]; putchar((c>=32&&c<127)?c:'.'); } printf("\n");
  }
  // Try decoding root node: terminalSize uleb, then childCount
  uint8_t*p=(uint8_t*)lt;
  uint64_t term=0; int sh=0;
  for(int i=0;i<10;i++){ uint8_t x=*p++; term|=(uint64_t)(x&0x7f)<<sh; if(!(x&0x80))break; sh+=7; }
  printf("root terminalSize=%llu next(childCount)=%u\n",(unsigned long long)term,*p);
  fflush(stdout);
  return 0;
}
