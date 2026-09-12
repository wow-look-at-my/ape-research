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
struct ctx{uint64_t trie;const char*want;int wlen;int found;uint64_t off;long steps;long nodes;};
static void walk(uint64_t node,char*pfx,int plen,struct ctx*c){
 if(c->found||plen>300||c->steps++>2000000)return;
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
   if(plen+(int)el<300 && !rd((uint64_t)p,pfx+plen,el)){ walk(c->trie+co,pfx,plen+(int)el,c); }
   p=q;
 }
}
static uint64_t lookup(uint64_t t,const char*n,long*nodes){
 if(!mapped(t)) return 0;
 struct ctx c={t,n,(int)strlen(n),0,0,0,0};char pfx[320];
 walk(t,pfx,0,&c);
 if(nodes)*nodes=c.nodes;
 return c.found?t+c.off:0;
}
int main(void){
 uint64_t slide=0x8830000ULL, ldvm=0x1ff05c000ULL, ldfo=0x4000ULL;
 // libdyld: hdr=0x18890f000 dataoff=0xb3cce0, want dlsym=0x188910c04
 uint64_t lt=ldvm+slide+(0xb3cce0ULL-ldfo);
 long nodes=0;
 printf("libdyld trie=%#llx mapped=%d\n",(unsigned long long)lt,mapped(lt)); fflush(stdout);
 uint64_t r=lookup(lt,"dlsym",&nodes);
 printf("  dlsym=%#llx (want 188910c04) nodes=%ld %s\n",(unsigned long long)r,nodes,r==0x188910c04ULL?"MATCH":"");
 // kernel: hdr=0x188cdd000 dataoff=0xb686d0, want getpid=0x188cde178
 uint64_t kt=ldvm+slide+(0xb686d0ULL-ldfo);
 printf("kernel trie=%#llx mapped=%d\n",(unsigned long long)kt,mapped(kt)); fflush(stdout);
 r=lookup(kt,"getpid",&nodes);
 printf("  getpid=%#llx (want 188cde178) nodes=%ld %s\n",(unsigned long long)r,nodes,r==0x188cde178ULL?"MATCH":"");
 r=lookup(kt,"mmap",&nodes);
 printf("  mmap=%#llx\n",(unsigned long long)r);
 // pthread: hdr=0x188d1b000 dataoff=0xb71498, want pthread_self=0x188d1d590
 uint64_t pt=ldvm+slide+(0xb71498ULL-ldfo);
 r=lookup(pt,"pthread_self",&nodes);
 printf("pthread trie=%#llx pthread_self=%#llx (want 188d1d590) %s\n",(unsigned long long)pt,(unsigned long long)r,r==0x188d1d590ULL?"MATCH":"");
 // libsystem_c: hdr=0x188bb0000 dataoff=0xb4dc50, want sysctl=0x188bb5944
 uint64_t ct=ldvm+slide+(0xb4dc50ULL-ldfo);
 r=lookup(ct,"sysctl",&nodes);
 printf("libsystem_c trie=%#llx sysctl=%#llx (want 188bb5944) %s\n",(unsigned long long)ct,(unsigned long long)r,r==0x188bb5944ULL?"MATCH":"");
 r=lookup(ct,"sysctlbyname",&nodes);
 printf("  sysctlbyname=%#llx\n",(unsigned long long)r);
 return 0;
}
