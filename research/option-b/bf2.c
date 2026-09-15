#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static int safe(uint64_t a,uint8_t*o){ if(!mapped(a)) return -1; *o=*(uint8_t*)a; return 0; }
static uint64_t uleb(const uint8_t**pp,int*ok){ uint64_t r=0;int s=0;const uint8_t*p=*pp;*ok=1;
 for(int i=0;i<10;i++){uint8_t b;if(safe((uint64_t)p,&b)){*ok=0;return 0;}p++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))break;s+=7;*pp=p;if(i==9)*ok=0;}
 *pp=p; return r; }
struct ctx{uint64_t trie;const char*want;int wlen;int found;uint64_t off;long steps;int bad;};
static void walk(const uint8_t*node,char*pfx,int plen,struct ctx*c){
 if(c->found||plen>300||c->steps++>500000){return;}
 const uint8_t*p=node;int ok;uint64_t term=uleb(&p,&ok);if(!ok){c->bad++;return;}
 if(term&&plen==c->wlen&&!memcmp(pfx,c->want,plen)){const uint8_t*a=p;uint64_t fl=uleb(&a,&ok);if(ok&&!(fl&0x08)){uint64_t o=uleb(&a,&ok);if(ok){c->off=o;c->found=1;return;}}}
 uint8_t nc;if(safe((uint64_t)p,&nc)){c->bad++;return;}p++;
 for(uint8_t i=0;i<nc;i++){
   const uint8_t*s=p;uint8_t b;
   while(!safe((uint64_t)s,&b)&&b)s++;
   uint64_t el=(uint64_t)(s-p);s++;
   uint64_t co=uleb(&s,&ok);if(!ok){c->bad++;return;}
   if(plen+(int)el<300){memcpy(pfx+plen,p,el);walk((const uint8_t*)(c->trie+co),pfx,plen+(int)el,c);}
   p=s;
 }
}
static uint64_t lookup(uint64_t trie,const char*n){
 struct ctx c={trie,n,(int)strlen(n),0,0,0,0};char pfx[320];
 walk((const uint8_t*)trie,pfx,0,&c);
 return c.found?trie+c.off:0;
}
int main(void){
 uint64_t lh=0x18890f000ULL; uint64_t want=0x188910c04ULL; // dlsym
 int found=0;
 for(uint64_t off=0; off<0x4000000 && found<5; off+=0x10){
   uint64_t t=lh+off;
   if(!mapped(t)) continue;
   uint8_t b0;if(safe(t,&b0))continue;
   if(b0>0x40) continue;   // root terminalSize uleb usually small
   struct ctx c={t,"dlsym",5,0,0,0,0};char pfx[320];
   walk((const uint8_t*)t,pfx,0,&c);
   if(c.found && t+c.off==want){
     printf("TRIE BASE FOUND=%#llx off=%#llx (dataoff=%#llx)\n",(unsigned long long)t,(unsigned long long)off,(unsigned long long)(t-lh));
     found++;
   }
 }
 printf("found=%d\n",found);
 return 0;
}
