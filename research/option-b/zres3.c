#include "rs.h"
// ========== zero-import resolver v3 (cache image table) ==========
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
#define LC_DYLD_EXPORTS_TRIE 0x80000033
#define LC_DYLD_INFO_ONLY 0x80000022

static int mapped(unsigned long a){ a&=~0x3fffUL; unsigned char v[2]; v[0]=0;
  if(rs3(78,a,16384,(long)v)!=0) return 0; return !(v[0]&0x80); }
static int rdb(unsigned long a,unsigned char*o){ if(!mapped(a))return -1; *o=*(volatile unsigned char*)a; return 0; }
static int uleb(unsigned long*a,unsigned long*v){ unsigned long r=0;int s=0;
  for(int i=0;i<10;i++){unsigned char b; if(rdb(*a,&b))return -1; (*a)++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80)){*v=r;return 0;} s+=7;} return -1; }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static int ismo(unsigned long p){ if(!mapped(p))return 0; if(*(volatile unsigned int*)p!=0xfeedfacf)return 0;
  struct mh64*h=(struct mh64*)p; return h->ncmds>0&&h->ncmds<2048&&h->sizeofcmds>32&&h->sizeofcmds<0x40000; }
// slide = runtime_addr_of_first_image - cache_vmaddr_of_first_image
static unsigned long cache_base(void){
  for(unsigned long a=0x180000000UL; a<0x1c0000000UL; a+=0x4000){ if(!mapped(a))continue;
    if(*(volatile unsigned int*)a==0x646c7964) return a; }
  return 0;
}
static unsigned long find_image(const char*want){
  unsigned long cb=cache_base(); if(!cb) return 0;
  unsigned char*b=(unsigned char*)cb;
  // imagesTextOffset at +0x88, count at +0x90; entry = {uuid[16], addr u64, size u32, pathOff u32}
  unsigned long ito=*(volatile unsigned long*)(b+0x88);
  unsigned long itc=*(volatile unsigned long*)(b+0x90);
  if(ito<0x100||ito>0x100000||itc<50||itc>20000) return 0;
  // Determine slide: first entry's unslid addr; find its runtime header by
  // scanning for a Mach-O at that page in the slid region... instead use the
  // known relation: images are mapped at cb + (entry.addr - cb_vm0).
  // The cache's dyldBaseAddress (unslid) is 0; the cache's own header lives
  // at cachebase, which corresponds to unslid 0x180000000.
  unsigned long slide = cb - 0x180000000UL;
  for(unsigned long i=0;i<itc;i++){
    unsigned char*e=b+ito+i*32;
    unsigned long addr=*(volatile unsigned long*)(e+16);
    unsigned int po=*(volatile unsigned int*)(e+28);
    if(!po||po>0x8000000||!mapped(cb+po)) continue;
    if(seq((const char*)(cb+po),want)) return addr+slide;
  }
  return 0;
}
static int walk(unsigned long node,char*pfx,int plen,const char*want,int wlen,unsigned long trie,unsigned long imghdr,unsigned long*out,long*budget){
  if(*budget<=0||!mapped(node)) return 0;
  unsigned long a=node,term,flags; (*budget)--;
  if(uleb(&a,&term)) return 0;
  if(term){ if(uleb(&a,&flags)) return 0;
    if(flags&0x08){ unsigned long ord; if(uleb(&a,&ord))return 0; unsigned long s=a; unsigned char b;
      while(!rdb(s,&b)&&b)s++; a=s+1; }
    else { unsigned long off; if(uleb(&a,&off)) return 0;
      if(plen==wlen&&!seq(pfx,want)){ *out=imghdr+off; return 1; } } }
  unsigned char nc; if(rdb(a,&nc)) return 0; a++;
  for(unsigned k=0;k<nc;k++){
    unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++;
    int el=(int)(s-a); s++;
    unsigned long co; if(uleb(&s,&co)) return 0;
    if(plen+el<250 && mapped(a) && mapped(a+el-1)){
      for(int i=0;i<el;i++) pfx[plen+i]=(char)*(volatile unsigned char*)(a+i);
      if(walk(trie+co,pfx,plen+el,want,wlen,trie,imghdr,out,budget)) return 1;
    }
    a=s;
  }
  return 0;
}
static unsigned long resolve(unsigned long imghdr,const char*sym){
  if(!ismo(imghdr)) return 0;
  struct mh64*h=(struct mh64*)imghdr; unsigned long p=imghdr+32,dataoff=0;
  for(unsigned int i=0;i<h->ncmds && i<4096;i++){
    if(!mapped(p)||!mapped(p+8)) break;
    struct lc*l=(struct lc*)p;
    if(l->cmd==LC_DYLD_EXPORTS_TRIE||l->cmd==LC_DYLD_INFO_ONLY){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; break; }
    if(l->cmdsize<8||l->cmdsize>0x40000) break; p+=l->cmdsize; }
  if(!dataoff) return 0;
  unsigned long trie=imghdr+dataoff; if(!mapped(trie)) return 0;
  char pfx[260]; unsigned long out=0; long budget=3000000;
  if(walk(trie,pfx,0,sym,slen(sym),trie,imghdr,&out,&budget)) return out;
  return 0;
}
static void puth(unsigned long v){o_lbl("0x");o_puth(v);}
int main(int argc,char**argv,char**envp){
  o_lbl("== zero-import resolver v3 ==\n");
  unsigned long cb=cache_base();
  o_lbl("cachebase="); puth(cb); o_lbl("\n");
  struct { const char*img; const char*sym; } T[]={
    {"/usr/lib/system/libsystem_kernel.dylib","_getpid"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mmap"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mach_vm_region"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_self"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_create"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctl"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctlbyname"},
    {"/usr/lib/system/libdyld.dylib","_dlsym"},
    {"/usr/lib/system/libdyld.dylib","_dlopen"},
    {"/usr/lib/system/libsystem_platform.dylib","_sys_icache_invalidate"},
    {"/usr/lib/system/libsystem_platform.dylib","_pthread_jit_write_protect_np"},
  };
  for(unsigned i=0;i<sizeof(T)/sizeof(T[0]);i++){
    o_lbl("searching "); o_puts(T[i].sym); o_lbl("...\n");
    unsigned long h=find_image(T[i].img);
    o_lbl("  hdr="); puth(h); o_lbl("\n");
    unsigned long r=h?resolve(h,T[i].sym):0;
    o_lbl(T[i].sym); o_lbl(" = "); puth(r);
    if(!h) o_lbl(" (image not found)");
    o_lbl("\n");
  }
  return 0;
}
