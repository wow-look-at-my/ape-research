#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
int main(void){
  // __LINKEDIT subcache: hdr @ 0x207888000, maps 0x1ff05c000 (size 0x23cf4000, fileOff 0x4000)
  // and 4K map at 0x1ff058000 fileOff 0.
  // kernel trie dataoff=0xb686d0. Try ld_base + (dataoff - ldfo)
  uint64_t ldhdr=0x207888000ULL; // where the header bytes live
  uint64_t ld_vm=0x1ff05c000ULL;
  uint64_t dataoff=0xb686d0ULL, ldfo=0x4000ULL;
  uint64_t c1=ld_vm+(dataoff-ldfo);
  uint64_t c2=0x1ff058000ULL+dataoff;
  uint64_t cands[]={c1,c2,ld_vm+dataoff,0};
  const char*nm[]={"ldvm+(off-ldfo)","4kmap+off","ldvm+off"};
  for(int i=0;cands[i];i++){
    uint8_t b[16]; int r=0;
    if(!mapped(cands[i])) r=-1; else memcpy(b,(void*)cands[i],16);
    printf("%-16s %#llx read=%s bytes=",nm[i],(unsigned long long)cands[i],r?"FAULT":"ok");
    if(!r) for(int k=0;k<16;k++) printf("%02x ",b[k]);
    printf("\n"); fflush(stdout);
  }
  // verify with libdyld: hdr=0x18890f000 dataoff=0xb3cce0 -> expect dlsym trie base such that
  // lookup gives 0x188910c04
  return 0;
}
