#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  printf("scanning for 'dyld_v1' subcache headers in mapped region:\n");
  for(uint64_t a=0x180000000ULL;a<0x1c0000000ULL;a+=0x1000){
    if(!mapped(a)) continue;
    unsigned char*b=(unsigned char*)a;
    if(b[0]=='d'&&b[1]=='y'&&b[2]=='l'&&b[3]=='d'&&b[4]=='_'&&b[5]=='v'&&b[6]=='1'){
      printf("  cache hdr @ %#llx magic=%.16s\n",(unsigned long long)a,b);
      fflush(stdout);
    }
  }
  return 0;
}
