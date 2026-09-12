#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static int rdb(uint64_t a,uint8_t*o){ if(!mapped(a)) return -1; *o=*(volatile uint8_t*)a; return 0; }
static int uleb(uint64_t*a,uint64_t*v){ uint64_t r=0; int s=0; for(int i=0;i<10;i++){ uint8_t b; if(rdb(*a,&b))return -1; (*a)++; r|=(uint64_t)(b&0x7f)<<s; if(!(b&0x80)){ *v=r; return 0;} s+=7; } return -1; }

static int g_dbg=0;
static int walk(uint64_t node,char*pfx,int plen,const char*want,int wlen,uint64_t*trie,uint64_t*out,int*depth_guard,uint64_t imghdr){
  static long budget;
  if(!*depth_guard){ budget=4000000; *depth_guard=1; }
  if(budget--<0) return 0;
  if(plen>256) return 0;
  if(!mapped(node)) return 0;
  uint64_t a=node;
  uint64_t term; if(uleb(&a,&term)) return 0;
  if(term){
    // terminal payload: flags uleb
    uint64_t flags; if(uleb(&a,&flags)) return 0;
    if(flags&0x08){ // reexport: ordinal, then imported name string
      uint64_t ord; if(uleb(&a,&ord)) return 0;
      uint64_t s=a; uint8_t b;
      while(!rdb(s,&b) && b) s++;
      a=s+1;
    } else {
      uint64_t off; if(uleb(&a,&off)) return 0;
      if(plen==wlen && !memcmp(pfx,want,plen)){
        if(g_dbg) printf("    [hit] node=%#llx off=%#llx\n",(unsigned long long)node,(unsigned long long)off);
        *out=imghdr+off; return 1;
      }
    }
  }
  uint8_t nc; if(rdb(a,&nc)) return 0; a++;
  if(nc>255) return 0;
  for(unsigned k=0;k<nc;k++){
    // edge: NUL-terminated string
    uint64_t s=a; uint8_t b;
    while(!rdb(s,&b) && b) s++;
    int el=(int)(s-a);
    s++;
    uint64_t co; if(uleb(&s,&co)) return 0;
    if(plen+el<256){
      if(!(mapped(a)&&mapped(a+el-1))) { a=s; continue; }
      memcpy(pfx+plen,(void*)a,el);
      if(walk((*trie)+co,pfx,plen+el,want,wlen,trie,out,depth_guard,imghdr)) return 1;
    }
    a=s;
  }
  return 0;
}
static uint64_t lookup(uint64_t trie,const char*name,uint64_t imghdr){
  if(!mapped(trie)) return 0;
  char pfx[300]; uint64_t out=0; int dg=0;
  if(walk(trie,pfx,0,name,(int)strlen(name),&trie,&out,&dg,imghdr)) return out;
  return 0;
}
int main(void){
  uint64_t slide=0x8830000ULL, ldvm=0x1ff05c000ULL, ldfo=0x4000ULL;
  struct { const char*img; uint64_t imghdr; uint32_t trie_off; const char*sym; uint64_t want; } T[]={
   {"libdyld",0x18890f000,0xb3cce0,"_dlsym",0x188910c04ULL},
   {"libdyld",0x18890f000,0xb3cce0,"_dyld_get_image_header",0x18891063cULL},
   {"libsystem_kernel",0x188cdd000,0xb686d0,"_getpid",0x188cde178ULL},
   {"libsystem_kernel",0x188cdd000,0xb686d0,"_mmap",0x188cde96cULL},
   {"libsystem_kernel",0x188cdd000,0xb686d0,"_mach_vm_region",0},
   {"libsystem_pthread",0x188d1b000,0xb71498,"_pthread_self",0x188d1d590ULL},
   {"libsystem_pthread",0x188d1b000,0xb71498,"_pthread_create",0},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_sysctl",0x188bb5944ULL},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_sysctlbyname",0},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_getentropy",0},
   {"libsystem_c",0x188bb0000,0xb4dc50,"_sem_open",0},
  };
  for(unsigned i=0;i<sizeof(T)/sizeof(T[0]);i++){
    g_dbg=1;
    uint64_t trie=ldvm+slide+((uint64_t)T[i].trie_off-ldfo);
    uint64_t r=lookup(trie,T[i].sym,T[i].imghdr);
    printf("%-18s %-26s -> %#llx",T[i].img,T[i].sym,(unsigned long long)r);
    if(T[i].want) printf("  want %#llx %s",(unsigned long long)T[i].want, r==T[i].want?"MATCH":"MISMATCH");
    printf("\n"); fflush(stdout);
  }
  return 0;
}
