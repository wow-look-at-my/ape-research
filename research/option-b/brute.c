#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static uint64_t uleb(const uint8_t**pp){ uint64_t r=0; int s=0; const uint8_t*p=*pp;
  for(int i=0;i<10;i++){ uint8_t b=*p++; r|=(uint64_t)(b&0x7f)<<s; if(!(b&0x80))break; s+=7; } *pp=p; return r; }
struct tctx{uint64_t base;const char*want;int wlen;int found;uint64_t off;long steps;};
static void twalk(const uint8_t*node,char*pfx,int plen,struct tctx*c){
  if(c->found||plen>240||c->steps++>200000) return;
  const uint8_t*p=node;
  uint64_t term=uleb(&p);
  if(term&&plen==c->wlen){ if(!memcmp(pfx,c->want,plen)){ const uint8_t*a=p; uint64_t flags=uleb(&a);
      if(!(flags&0x08)){ c->off=uleb(&a); c->found=1; return; } } }
  uint64_t nc=*p++;
  if(nc>4096) return;
  for(uint64_t i=0;i<nc;i++){
    const uint8_t*s=p; while(*s)s++; uint64_t el=(uint64_t)(s-p); s++;
    uint64_t co=uleb(&s);
    if(plen+(int)el<240){ memcpy(pfx+plen,p,el);
      if(mapped(c->base+co)) twalk((const uint8_t*)(c->base+co),pfx,plen+(int)el,c); }
    p=s;
  }
}
static uint64_t lookup(uint64_t trie,const char*name){
  struct tctx c={trie,name,(int)strlen(name),0,0,0}; char pfx[256];
  twalk((const uint8_t*)trie,pfx,0,&c);
  return c.found?c.off:0;
}
int main(void){
  // brute-force trie base over the cache for "getpid"; kernel hdr=0x188cdd000,
  // expected hdr+off == 0x188cde178 => off 0xf34
  uint64_t kh=0x188cdd000ULL;
  int hits=0;
  for(uint64_t b=0x188000000ULL; b<0x192000000ULL && hits<8; b+=0x100){
    if(!mapped(b)) continue;
    // cheap prefilter: root node -> terminal uleb + childCount>0
    const uint8_t*p=(const uint8_t*)b;
    uint64_t t=uleb(&p);
    if(t>100000) continue;
    uint64_t nc=*p;
    if(nc==0) continue;
    uint64_t off=lookup(b,"getpid");
    if(off && (kh+off)==0x188cde178ULL){
      printf("TRIE FOUND base=%#llx getpid_off=%#llx hdr+off=%#llx\n",(unsigned long long)b,(unsigned long long)off,(unsigned long long)(kh+off));
      hits++;
    }
  }
  printf("done hits=%d\n",hits);
  return 0;
}
