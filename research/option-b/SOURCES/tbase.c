#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static int rd(uint64_t a,void*o,uint64_t n){ if(!mapped(a)||!mapped(a+n-1)) return -1; memcpy(o,(void*)a,n); return 0; }
static uint64_t uleb(const uint8_t**pp,int*ok){uint64_t r=0;int s=0;const uint8_t*p=*pp;*ok=1;
 for(int i=0;i<10;i++){uint8_t b;if(rd((uint64_t)p,&b,1)){*ok=0;return 0;}p++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80)){*pp=p;return r;}s+=7;}
 *ok=0;return 0;}
struct ctx{uint64_t trie;const char*want;int wlen;int found;uint64_t off;long steps;long nodes;};
static void walk(uint64_t node,char*pfx,int plen,struct ctx*c){
 if(c->found||plen>300||c->steps++>800000)return;
 if(!mapped(node))return;
 const uint8_t*p=(const uint8_t*)node;int ok;
 uint64_t term=uleb(&p,&ok); if(!ok)return;
 c->nodes++;
 if(term&&plen==c->wlen&&!memcmp(pfx,c->want,plen)){
   uint64_t fl=uleb(&p,&ok); if(ok&&!(fl&0x08)){uint64_t o=uleb(&p,&ok); if(ok){c->found=1;c->off=o;return;}}}
 uint8_t nc; if(rd((uint64_t)p,&nc,1))return; p++;
 for(uint8_t i=0;i<nc;i++){
   uint64_t s=(uint64_t)p;uint8_t b;
   while(1){ if(rd(s,&b,1))return; if(!b)break; s++; }
   uint64_t el=s-(uint64_t)p; s++;
   const uint8_t*q=(const uint8_t*)s;
   uint64_t co=uleb(&q,&ok); if(!ok)return;
   if(plen+(int)el<300){ memcpy(pfx+plen,(void*)p,el); walk(c->trie+co,pfx,plen+(int)el,c); }
   p=q;
 }
}
static uint64_t lookup(uint64_t t,const char*n){
 struct ctx c={t,n,(int)strlen(n),0,0,0,0};char pfx[320];
 walk(t,pfx,0,&c);
 printf("    lookup %s: found=%d off=%#llx nodes=%ld steps=%ld\n",n,c.found,(unsigned long long)c.off,c.nodes,c.steps);
 return c.found?t+c.off:0;
}
int main(void){
 uint64_t lh=0x18890f000ULL, dataoff=0xb3cce0ULL, slide=0x8830000ULL;
 uint64_t cachebase=0x188830000ULL;
 uint64_t bases[]={cachebase+dataoff, lh+dataoff, 0x190340000ULL+dataoff, 0};
 const char*nm[]={"cachebase+off","hdr+off","textvm+off"};
 for(int i=0;bases[i];i++){
   uint8_t b[16]; int r=rd(bases[i],b,16);
   printf("BASE %-16s %#llx read=%s bytes=",nm[i],(unsigned long long)bases[i],r?"FAULT":"ok");
   if(!r) for(int k=0;k<16;k++) printf("%02x ",b[k]);
   printf("\n"); fflush(stdout);
   if(r) continue;
   uint64_t r2=lookup(bases[i],"dlsym");
   printf("    -> dlsym=%#llx (want 188910c04) %s\n",(unsigned long long)r2,r2==0x188910c04ULL?"MATCH":"");
 }
 return 0;
}
