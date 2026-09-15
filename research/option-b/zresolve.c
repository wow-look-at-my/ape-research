#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static int rd(uint64_t a,void*o,uint64_t n){ if(!mapped(a)||!mapped(a+n-1)) return -1; memcpy(o,(void*)a,n); return 0; }
static uint64_t uleb(const uint8_t**pp,int*ok){uint64_t r=0;int s=0;const uint8_t*p=*pp;*ok=1;
 for(int i=0;i<10;i++){uint8_t b;if(rd((uint64_t)p,&b,1)){*ok=0;return 0;}p++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80)){*pp=p;return r;}s+=7;}
 *ok=0;return 0;}
struct ctx{uint64_t trie;const char*want;int wlen;int found;int reexport;uint64_t off;long steps;};
static void walk(uint64_t node,char*pfx,int plen,struct ctx*c){
 if(c->found||plen>300||c->steps++>3000000)return;
 if(!mapped(node))return;
 const uint8_t*p=(const uint8_t*)node;int ok;
 uint64_t term=uleb(&p,&ok); if(!ok)return;
 if(term){
   if(plen==c->wlen&&!memcmp(pfx,c->want,plen)){
     uint64_t fl=uleb(&p,&ok); if(ok){
       if(fl&0x08){ c->reexport=1; c->found=1; return; }
       uint64_t o=uleb(&p,&ok); if(ok){c->found=1;c->off=o;return;} } }
   // skip terminal payload
   const uint8_t*q=p; uint64_t fl=uleb(&q,&ok); if(!ok)return;
   if(fl&0x08){ uint64_t ord=uleb(&q,&ok); if(!ok)return; (void)ord; while(1){uint8_t b;if(rd((uint64_t)q,&b,1))return;q++;if(!b)break;} }
   p=q;
 }
 uint8_t nc; if(rd((uint64_t)p,&nc,1))return; p++; if(nc>200) return;
 for(uint8_t i=0;i<nc;i++){
   uint64_t s=(uint64_t)p;uint8_t b;
   while(1){ if(rd(s,&b,1))return; if(!b)break; s++; }
   uint64_t el=s-(uint64_t)p; s++;
   const uint8_t*q=(const uint8_t*)s;
   uint64_t co=uleb(&q,&ok); if(!ok)return;
   if(plen+(int)el<300 && !rd((uint64_t)p,pfx+plen,el)) walk(c->trie+co,pfx,plen+(int)el,c);
   p=q;
 }
}
static uint64_t lookup(uint64_t t,const char*n,int*re){
 if(!mapped(t)) return 0;
 struct ctx c={t,n,(int)strlen(n),0,0,0,0};char pfx[320];
 walk(t,pfx,0,&c);
 if(re)*re=c.reexport;
 return c.found&&!c.reexport?t+c.off:0;
}
int main(void){
  uint64_t slide=0x8830000ULL;
  uint64_t ldvm=0x1ff05c000ULL, ldfo=0x4000ULL;
  struct { const char*img; uint32_t hdr; uint32_t trie; const char*sym; uint64_t want; } T[]={
   {"libdyld",0x18890f000,0xb3cce0,"_dlsym",0x188910c04ULL},
   {"libsystem_kernel",0x188cdd000,0xb686d0,"_getpid",0x188cde178ULL},
   {"libsystem_kernel",0x188cdd000,0xb686d0,"_mmap",0x188cde96cULL},
   {"libsystem_pthread",0x188d1b000,0xb71498,"_pthread_self",0x188d1d590ULL},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_sysctl",0x188bb5944ULL},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_sysctlbyname",0},
  };
  for(unsigned i=0;i<sizeof(T)/sizeof(T[0]);i++){
    uint64_t trie=ldvm+slide+((uint64_t)T[i].trie-ldfo);
    int re=0; uint64_t r=lookup(trie,T[i].sym,&re);
    printf("%-18s %-16s trie=%#llx -> %#llx",T[i].img,T[i].sym,(unsigned long long)trie,(unsigned long long)r);
    if(T[i].want) printf("  want %#llx %s",(unsigned long long)T[i].want, r==T[i].want?"MATCH":"MISMATCH");
    else printf("  reexport=%d",re);
    printf("\n"); fflush(stdout);
  }
  return 0;
}
