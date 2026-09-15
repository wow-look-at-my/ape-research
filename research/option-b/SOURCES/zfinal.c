#include "rs.h"
/* ================== Option B: zero-import symbol resolver ==================
 * No imported symbols. Locates libSystem's image via the dyld shared cache
 * image table (raw mincore syscall to test mappedness), then resolves any
 * exported symbol through the image's dyld export trie.
 *
 * Address model (split dyld shared cache):
 *   __TEXT.vmaddr and __LINKEDIT.vmaddr are UNSLID cache vmaddrs.
 *   slide            = runtime_image_header - __TEXT.vmaddr
 *   trie_runtime     = __LINKEDIT.vmaddr + slide + (dataoff - __LINKEDIT.fileoff)
 *   symbol_runtime   = image_header + export_offset   (export offsets are
 *                      image-relative, so this needs no slide at all)
 * ======================================================================== */
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct seg64 { unsigned int cmd,cmdsize; char segname[16]; unsigned long vmaddr,vmsize,fileoff,filesize; int maxprot,initprot; unsigned int nsects,flags; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
#define LC_SEGMENT_64 0x19
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
static unsigned long cache_base(void){
  for(unsigned long a=0x180000000UL; a<0x1c0000000UL; a+=0x4000){ if(!mapped(a))continue;
    if(*(volatile unsigned int*)a==0x646c7964) return a; }
  return 0;
}
/* locate image by LC_ID_DYLIB install name using the cache image table */
static unsigned long find_image(const char*want){
  unsigned long cb=cache_base(); if(!cb) return 0;
  unsigned char*b=(unsigned char*)cb;
  unsigned long ito=*(volatile unsigned long*)(b+0x88);
  unsigned long itc=*(volatile unsigned long*)(b+0x90);
  if(ito<0x100||ito>0x100000||itc<50||itc>20000) return 0;
  unsigned long slide=cb-0x180000000UL;
  for(unsigned long i=0;i<itc;i++){
    unsigned char*e=b+ito+i*32;
    unsigned long addr=*(volatile unsigned long*)(e+16);
    unsigned int po=*(volatile unsigned int*)(e+28);
    if(!po||po>0x8000000||!mapped(cb+po)) continue;
    if(seq((const char*)(cb+po),want)) return addr+slide;
  }
  return 0;
}
/* parse load commands; returns dataoff and __LINKEDIT vmaddr/fileoff and slide */
static int parse(unsigned long imghdr,unsigned long*dataoff,unsigned long*ldvm,unsigned long*ldfo,unsigned long*slide){
  struct mh64*h=(struct mh64*)imghdr; unsigned long p=imghdr+32;
  unsigned long textvm=0; *dataoff=0; *ldvm=0; *ldfo=0;
  for(unsigned int i=0;i<h->ncmds && i<4096;i++){
    if(!mapped(p)||!mapped(p+8)) return 0;
    struct lc*l=(struct lc*)p;
    if(l->cmd==LC_SEGMENT_64){ struct seg64*s=(struct seg64*)p;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='T'&&s->segname[3]=='E'&&s->segname[4]=='X'&&s->segname[5]=='T') textvm=s->vmaddr;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='L'&&s->segname[3]=='I') { *ldvm=s->vmaddr; *ldfo=s->fileoff; } }
    if((l->cmd==LC_DYLD_EXPORTS_TRIE||l->cmd==LC_DYLD_INFO_ONLY) && !*dataoff){ struct ldc*d=(struct ldc*)p; *dataoff=d->dataoff; }
    if(l->cmdsize<8||l->cmdsize>0x40000) break;
    p+=l->cmdsize;
  }
  if(!textvm||!*dataoff) return 0;
  *slide=imghdr-textvm;
  return 1;
}
static long g_budget; static long g_nodes;
static int walk(unsigned long node,char*pfx,int plen,const char*want,int wlen,unsigned long trie,unsigned long imghdr,unsigned long*out){
  if(g_budget<=0||plen>250||!mapped(node)) return 0;
  g_budget--; g_nodes++;
  unsigned long a=node,term,flags;
  if(uleb(&a,&term)) return 0;
  if(term){
    if(uleb(&a,&flags)) return 0;
    if(flags&0x08){ unsigned long ord; if(uleb(&a,&ord))return 0; unsigned long s=a; unsigned char b;
      while(!rdb(s,&b)&&b)s++; a=s+1; }
    else { unsigned long off; if(uleb(&a,&off)) return 0;
      if(plen==wlen){ pfx[plen]=0; if(seq(pfx,want)){ *out=imghdr+off; return 1; } } } }
  unsigned char nc; if(rdb(a,&nc)) return 0; a++;
  for(unsigned k=0;k<nc;k++){
    unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++;
    int el=(int)(s-a); s++;
    unsigned long co; if(uleb(&s,&co)) return 0;
    if(plen+el<250 && mapped(a) && mapped(a+el-1)){
      for(int i=0;i<el;i++) pfx[plen+i]=(char)*(volatile unsigned char*)(a+i);
      if(walk(trie+co,pfx,plen+el,want,wlen,trie,imghdr,out)) return 1;
    }
    a=s;
  }
  return 0;
}
static unsigned long resolve_one(unsigned long imghdr,const char*sym){
  if(!imghdr) return 0;
  unsigned long dataoff,ldvm,ldfo,slide;
  if(!parse(imghdr,&dataoff,&ldvm,&ldfo,&slide)) return 0;
  unsigned long trie=ldvm+slide+(dataoff-ldfo);
  if(!mapped(trie)) return 0;
  char pfx[260]; unsigned long out=0;
  g_budget=3000000; g_nodes=0;
  if(walk(trie,pfx,0,sym,slen(sym),trie,imghdr,&out)) return out;
  return 0;
}
static unsigned long resolve(const char*image,const char*sym){
  unsigned long h=resolve_one(find_image(image),sym);
  if(h) return h;
  if(1) return 0;  /* primary-route test; fallback measured separately */
  /* fallback: re-exported symbols live in another cache image; search all */
  unsigned long cb=cache_base(); if(!cb) return 0;
  unsigned char*b=(unsigned char*)cb;
  unsigned long ito=*(volatile unsigned long*)(b+0x88);
  unsigned long itc=*(volatile unsigned long*)(b+0x90);
  if(ito<0x100||ito>0x100000||itc<50||itc>20000) return 0;
  unsigned long slide=cb-0x180000000UL;
  for(unsigned long i=0;i<itc;i++){
    unsigned char*e=b+ito+i*32;
    unsigned long addr=*(volatile unsigned long*)(e+16)+slide;
    if(!ismo(addr)) continue;
    unsigned long r=resolve_one(addr,sym);
    if(r) return r;
  }
  return 0;
}
static void puth(unsigned long v){o_lbl("0x");o_puth(v);}
int main(int argc,char**argv,char**envp){
  o_lbl("== zero-import resolver (final) ==\n");
  unsigned long cb=cache_base();
  o_lbl("cachebase="); puth(cb); o_lbl("\n");
  struct { const char*img; const char*sym; } T[]={
    {"/usr/lib/system/libsystem_kernel.dylib","_getpid"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mmap"},
    {"/usr/lib/system/libsystem_kernel.dylib","_munmap"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mprotect"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mach_vm_region"},
    {"/usr/lib/system/libsystem_kernel.dylib","_fork"},
    {"/usr/lib/system/libsystem_kernel.dylib","_openat"},
    {"/usr/lib/system/libsystem_kernel.dylib","_close"},
    {"/usr/lib/system/libsystem_kernel.dylib","_write"},
    {"/usr/lib/system/libsystem_kernel.dylib","_read"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sigaction"},
    {"/usr/lib/system/libsystem_kernel.dylib","_pselect"},
    {"/usr/lib/system/libsystem_kernel.dylib","_pipe"},
    {"/usr/lib/system/libsystem_kernel.dylib","_getentropy"},
    {"/usr/lib/system/libsystem_kernel.dylib","_raise"},
    {"/usr/lib/system/libsystem_kernel.dylib","_getrlimit"},
    {"/usr/lib/system/libsystem_kernel.dylib","_setrlimit"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_open"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_unlink"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_close"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_post"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_wait"},
    {"/usr/lib/system/libsystem_kernel.dylib","_sem_trywait"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_self"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_create"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_kill"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_sigmask"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_setname_np"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_exit"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_join"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_yield_np"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_attr_init"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_attr_destroy"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_attr_setstacksize"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_attr_setguardsize"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_cpu_number_np"},
    {"/usr/lib/system/libsystem_platform.dylib","_sys_icache_invalidate"},
    {"/usr/lib/system/libsystem_platform.dylib","_pthread_jit_write_protect_np"},
    {"/usr/lib/system/libsystem_platform.dylib","_pthread_jit_write_protect_supported_np"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctl"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctlbyname"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctlnametomib"},
    {"/usr/lib/system/libsystem_c.dylib","_clock_gettime"},
    {"/usr/lib/system/libsystem_c.dylib","_nanosleep"},
    {"/usr/lib/system/libsystem_c.dylib","_sigaltstack"},
    {"/usr/lib/system/libdyld.dylib","_dlsym"},
    {"/usr/lib/system/libdyld.dylib","_dlopen"},
    {"/usr/lib/system/libdyld.dylib","_dlclose"},
    {"/usr/lib/system/libdyld.dylib","_dlerror"},
    {"/usr/lib/system/libdispatch.dylib","_dispatch_semaphore_create"},
    {"/usr/lib/system/libdispatch.dylib","_dispatch_semaphore_signal"},
    {"/usr/lib/system/libdispatch.dylib","_dispatch_semaphore_wait"},
    {"/usr/lib/system/libdispatch.dylib","_dispatch_walltime"},
  };
  for(unsigned i=0;i<sizeof(T)/sizeof(T[0]);i++){
    unsigned long r=resolve(T[i].img,T[i].sym);
    o_lbl(T[i].sym); o_lbl(" = "); puth(r); o_lbl("\n");
  }
  return 0;
}
