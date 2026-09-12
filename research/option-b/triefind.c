#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
// An export trie edge is <len><chars><childOffset uleb>. Search for
// "<len>dlsym<uleb>" and for "<len>getpid<uleb>", then relate to headers.
int main(void){
  const char*w[]={"dlsym","getpid","sysctl","pthread_self",0};
  for(int wi=0;w[wi];wi++){
    int L=(int)strlen(w[wi]);
    printf("=== %s (len %d) ===\n",w[wi],L);
    int hits=0;
    for(uint64_t a=0x180000000ULL;a<0x220000000ULL && hits<12;a+=0x4000){
      if(!mapped(a)) continue;
      unsigned char*pg=(unsigned char*)a;
      for(int i=0;i<16384-8;i++){
        if(pg[i]!=(unsigned char)L) continue;
        if(memcmp(pg+i+1,w[wi],L)) continue;
        // followed by a uleb child offset (1-3 bytes)
        printf("  @ %#llx : %02x %02x %02x %02x %02x  (len=%d)\n",
          (unsigned long long)(a+i), pg[i],pg[i+1],pg[i+2],pg[i+3],pg[i+4], pg[i]);
        hits++;
        if(hits>=12) break;
      }
    }
    fflush(stdout);
  }
  return 0;
}
