#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  uint64_t base=0x188830000ULL; // first cache header
  unsigned char*b=(unsigned char*)base;
  // scan cache header words 0..0x400 for (offset,count) pairs whose entries have valid paths
  for(int i=8;i<0x400/8-1;i++){
    uint64_t off=*(uint64_t*)(b+i*8);
    uint64_t cnt=*(uint64_t*)(b+(i+1)*8);
    if(off<0x100||off>0x100000) continue;
    if(cnt<100||cnt>8000) continue;
    if(!mapped(base+off)) continue;
    // entry 0: addr, modTime, inode, pathFileOffset(24)
    uint64_t pfo=*(uint32_t*)(b+off+24);
    if(pfo<0x100||pfo>0x8000000) continue;
    if(!mapped(base+pfo)) continue;
    if(*(char*)(b+pfo)!='/') continue;
    printf("table candidate: off=%#llx count=%llu at word %d (+%#x); path0=%.60s\n",
      (unsigned long long)off,(unsigned long long)cnt,i,i*8,(char*)(b+pfo));
    // check a few more
    int ok=0;
    for(int k=0;k<5 && k<(int)cnt;k++){
      uint64_t p=*(uint32_t*)(b+off+k*32+24);
      if(mapped(base+p)&&*(char*)(b+p)=='/') ok++;
    }
    printf("   valid paths in first 5: %d\n",ok);
  }
  return 0;
}
