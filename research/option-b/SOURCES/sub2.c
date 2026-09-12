#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  printf("scan 0x180000000..0x2c0000000 for dyld_v1 headers:\n");
  int n=0;
  for(uint64_t a=0x180000000ULL;a<0x2c0000000ULL;a+=0x4000){
    if(!mapped(a)) continue;
    unsigned char*b=(unsigned char*)a;
    if(b[0]=='d'&&b[1]=='y'&&b[2]=='l'&&b[3]=='d'&&b[4]=='_'&&b[5]=='v'&&b[6]=='1'){
      uint32_t mo=*(uint32_t*)(b+16), mc=*(uint32_t*)(b+20);
      printf("  hdr @ %#llx magic=%.16s mappingOffset=%u mappingCount=%u\n",(unsigned long long)a,b,mo,mc);
      for(uint32_t i=0;i<mc && i<12;i++){
        unsigned char*m=b+mo+i*32;
        uint64_t addr=*(uint64_t*)(m),size=*(uint64_t*)(m+8),fo=*(uint64_t*)(m+16);
        printf("      map[%u] addr=%#llx size=%#llx fileOff=%#llx\n",i,(unsigned long long)addr,(unsigned long long)size,(unsigned long long)fo);
      }
      n++;
      if(n>8) return 0;
    }
  }
  printf("total=%d\n",n);
  return 0;
}
