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
struct ctx{uint64_t trie;const char*want;int wlen;int found;uint64_t off;long steps;};
static void walk(uint64_t node,char*pfx,int plen,struct ctx*c){
 if(c->found||plen>300||c->steps++>2000000)return;
 const uint8_t*p=(const uint8_t*)node;int ok;
 uint64_t term=uleb(&p,&ok); if(!ok)return;
 if(term&&plen==c->wlen&&!memcmp(pfx,c->want,plen)){
   uint64_t fl=uleb(&p,&ok); if(ok&&!(fl&0x08)){uint64_t o=uleb(&p,&ok); if(ok){c->found=1;c->off=o;return;}}}
 uint8_t nc; if(rd((uint64_t)p,&nc,1))return; p++;
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
static uint64_t lookup(uint64_t t,const char*n){
 if(!mapped(t)) return 0;
 struct ctx c={t,n,(int)strlen(n),0,0,0};char pfx[320];
 walk(t,pfx,0,&c);
 return c.found?t+c.off:0;
}
int main(void){
  uint64_t slides[]={0x8830000ULL,0x8828000ULL,0x88b0000ULL};
  uint64_t ldvm=0x1ff05c000ULL, ldfo=0x4000ULL, off=0xb3cce0ULL;
  for(int i=0;i<3;i++){
    uint64_t t=ldvm+slides[i]+(off-ldfo);
    uint8_t b[12]; int r=rd(t,b,12);
    printf("slide=%#llx trie=%#llx mapped=%d",(unsigned long long)slides[i],(unsigned long long)t,mapped(t));
    if(!r){ printf(" bytes="); for(int k=0;k<12;k++) printf("%02x ",b[k]); }
    printf("\n"); fflush(stdout);
    if(!r){ uint64_t d=lookup(t,"dlsym"); printf("   dlsym=%#llx %s\n",(unsigned long long)d,d==0x188910c04ULL?"MATCH":""); fflush(stdout); }
  }
  return 0;
}
