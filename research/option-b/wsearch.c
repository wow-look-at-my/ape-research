#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
// Search for a byte string across the mapped address space, page by page.
int main(void){
  const char*needle="getpid";
  int nl=6;
  uint64_t lo=0x180000000ULL, hi=0x1c0000000ULL;
  int hits=0;
  for(uint64_t a=lo;a<hi && hits<40;a+=0x4000){
    if(!mapped(a)) continue;
    unsigned char*pg=(unsigned char*)a;
    // search within this 16K page (may span into next; check both)
    for(int i=0;i<16384-nl;i++){
      if(pg[i]==needle[0] && !memcmp(pg+i,needle,nl)){
        // require preceding byte to be non-ident (trie edge strings) or any
        printf("found \"%s\" at %#llx\n", needle, (unsigned long long)(a+i));
        hits++;
        if(hits>=40) break;
      }
    }
  }
  printf("hits=%d\n",hits);
  return 0;
}
