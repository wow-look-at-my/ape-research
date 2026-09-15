#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static int rd(const uint8_t*p,uint8_t*out){ if(!mapped((uint64_t)p)) return -1; *out=*p; return 0; }
static uint64_t uleb2(const uint8_t**pp,int*ok){ uint64_t r=0; int s=0; const uint8_t*p=*pp; *ok=1;
  for(int i=0;i<10;i++){ uint8_t b; if(rd(p,&b)){*ok=0;return 0;} p++; r|=(uint64_t)(b&0x7f)<<s; if(!(b&0x80))break; s+=7; if(i==9){*ok=0;return 0;} }
  *pp=p; return r; }
struct ctx{uint64_t trie;const char*want;int wlen;int found;uint64_t off;long steps;};
static void walk(const uint8_t*node,char*pfx,int plen,struct ctx*c){
  if(c->found||plen>300||c->steps++>2000000) return;
  if(!mapped((uint64_t)node)) return;
  const uint8_t*p=node; int ok;
  uint64_t term=uleb2(&p,&ok); if(!ok) return;
  if(term&&plen==c->wlen && !memcmp(pfx,c->want,plen)){
    const uint8_t*a=p; uint64_t flags=uleb2(&a,&ok); if(!ok) return;
    if(!(flags&0x08)){ uint64_t off=uleb2(&a,&ok); if(ok){ c->off=off; c->found=1; return; } } }
  uint8_t nc; if(rd(p,&nc)) return; p++;
  for(uint8_t i=0;i<nc;i++){
    if(!mapped((uint64_t)p)) return;
    const uint8_t*s=p;
    while(1){ uint8_t b; if(rd(s,&b)) return; if(!b) break; s++; }
    uint64_t el=(uint64_t)(s-p); s++;
    uint64_t co=uleb2(&s,&ok); if(!ok) return;
    if(plen+(int)el<300){ memcpy(pfx+plen,p,el); walk((const uint8_t*)(c->trie+co),pfx,plen+(int)el,c); }
    p=s;
  }
}
static uint64_t lookup(uint64_t trie,const char*name){
  struct ctx c={trie,name,(int)strlen(name),0,0,0}; char pfx[320];
  walk((const uint8_t*)trie,pfx,0,&c);
  return c.found?trie+c.off:0;
}
int main(void){
  // libdyld hdr=0x18890f000, dataoff=0xb3cce0 => trie 0x18944bce0
  // expected: _dlsym at hdr+0x1C04 = 0x188910c04
  uint64_t lh=0x18890f000ULL, lt=lh+0xb3cce0ULL;
  printf("libdyld trie=%#llx mapped=%d\n",(unsigned long long)lt,mapped(lt));
  { const uint8_t*b=(const uint8_t*)lt; printf("  bytes:"); for(int i=0;i<10;i++) printf(" %02x",b[i]); printf("\n"); }
  uint64_t r=lookup(lt,"dlsym");
  printf("dlsym via trie = %#llx (expect 188910c04) %s\n",(unsigned long long)r, r==0x188910c04ULL?"MATCH":"");
  // kernel hdr=0x188cdd000 dataoff=0xb686d0 => trie 0x1898456d0, getpid=hdr+0xf34
  uint64_t kh=0x188cdd000ULL, kt=kh+0xb686d0ULL;
  r=lookup(kt,"getpid");
  printf("getpid via trie = %#llx (expect 188cde178) %s\n",(unsigned long long)r, r==0x188cde178ULL?"MATCH":"");
  r=lookup(kt,"mmap");
  printf("mmap via trie = %#llx\n",(unsigned long long)r);
  // pthread
  uint64_t ph=0x188d1b000ULL, pt=ph+0xb71498ULL;
  r=lookup(pt,"pthread_self");
  printf("pthread_self via trie = %#llx (expect 188d1d590) %s\n",(unsigned long long)r, r==0x188d1d590ULL?"MATCH":"");
  // libsystem_c sysctl
  uint64_t ch=0x188bb0000ULL, ct=ch+0xb4dc50ULL;
  r=lookup(ct,"sysctl");
  printf("sysctl via trie = %#llx (expect 188bb5944) %s\n",(unsigned long long)r, r==0x188bb5944ULL?"MATCH":"");
  r=lookup(ct,"sysctlbyname");
  printf("sysctlbyname via trie = %#llx\n",(unsigned long long)r);
  return 0;
}
