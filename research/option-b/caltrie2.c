#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <sys/syscall.h>
#include <unistd.h>
struct mh64 { uint32_t magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { uint32_t cmd, cmdsize; };
struct ldc { uint32_t cmd, cmdsize, dataoff, datasize; };
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static uint64_t uleb(const uint8_t**pp){ uint64_t r=0; int s=0; const uint8_t*p=*pp;
  for(int i=0;i<10;i++){ uint8_t b=*p++; r|=(uint64_t)(b&0x7f)<<s; if(!(b&0x80))break; s+=7; } *pp=p; return r; }
struct tctx{uint64_t base;const char*want;int wlen;int found;uint64_t off;long steps;};
static void twalk(const uint8_t*node,char*pfx,int plen,struct tctx*c){
  if(c->found||plen>240||c->steps++>5000000) return;
  const uint8_t*p=node;
  uint64_t term=uleb(&p);
  if(term&&plen==c->wlen){
    if(!memcmp(pfx,c->want,plen)){ const uint8_t*a=p; uint64_t flags=uleb(&a);
      if(!(flags&0x08)){ c->off=uleb(&a); c->found=1; return; } } }
  uint64_t nc=*p++;
  for(uint64_t i=0;i<nc;i++){
    const uint8_t*s=p; while(*s)s++; uint64_t el=(uint64_t)(s-p); s++;
    uint64_t co=uleb(&s);
    if(plen+(int)el<240){ memcpy(pfx+plen,p,el);
      if(mapped(c->base+co)) twalk((const uint8_t*)(c->base+co),pfx,plen+(int)el,c); }
    p=s;
  }
}
static uint64_t lookup(uint64_t trie,const char*name){
  if(!mapped(trie)) return 0;
  struct tctx c={trie,name,(int)strlen(name),0,0,0}; char pfx[256];
  twalk((const uint8_t*)trie,pfx,0,&c);
  return c.found?trie+c.off:0;
}
int main(void){
  uint64_t kh=0x188cdd000ULL; uint32_t kdo=0xb686d0;
  // print header bytes of each candidate
  uint64_t cands[4]={kh+kdo, 0x1804ad000ULL+kdo, 0x188830000ULL+kdo, 0x1ff05c000ULL+(kdo-0x4000)};
  const char*nm[4]={"hdr+off","textvm+off","cachebase+off","ldvm+off-ldfo"};
  for(int i=0;i<4;i++){
    uint64_t t=cands[i];
    printf("%-22s %#llx mapped=%d", nm[i],(unsigned long long)t, mapped(t));
    if(mapped(t)){ uint8_t*b=(uint8_t*)t; printf(" first:"); for(int k=0;k<8;k++) printf(" %02x",b[k]); }
    printf("\n"); fflush(stdout);
  }
  printf("getpid via hdr+off = %#llx (want 0x188cde178)\n",(unsigned long long)lookup(kh+kdo,"getpid")); fflush(stdout);
  return 0;
}
