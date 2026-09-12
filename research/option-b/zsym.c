#include "rs.h"
// ---------- zero-import symbol resolution ----------
struct mh64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { unsigned int cmd, cmdsize; };
struct dylibc { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct ldc { unsigned int cmd, cmdsize, dataoff, datasize; };
#define LC_ID_DYLIB 0xd
#define LC_DYLD_EXPORTS_TRIE 0x80000033
#define LC_DYLD_INFO_ONLY 0x80000022

static int mm(unsigned long a){ a &= ~0x3fffUL; if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int ismo(unsigned long p){ if(!mm(p))return 0; if(*(volatile unsigned int*)p!=0xfeedfacf) return 0; struct mh64*h=(struct mh64*)p; return h->ncmds>0&&h->ncmds<8192&&h->sizeofcmds>0&&h->sizeofcmds<0x40000; }
static unsigned long uleb(const unsigned char**pp){ unsigned long r=0; int s=0; const unsigned char*p=*pp;
  for(int i=0;i<10;i++){ unsigned char b=*p++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80))break; s+=7; } *pp=p; return r; }

struct tctx { unsigned long base; const char*want; int wlen; int found; unsigned long off; int steps; };
static void twalk(const unsigned char*node, char*prefix, int plen, struct tctx*c){
  if(c->found || plen>240 || c->steps++>400000) return;
  const unsigned char*p=node;
  unsigned long terminal=uleb(&p);
  if(terminal && plen==c->wlen){
    int ok=1; for(int i=0;i<plen;i++) if(prefix[i]!=c->want[i]){ok=0;break;}
    if(ok){ const unsigned char*a=p; unsigned long flags=uleb(&a);
      if(!(flags&0x08)){ unsigned long off=uleb(&a); c->found=1; c->off=off; } }
  }
  unsigned long nchild=*p++;
  for(unsigned long i=0;i<nchild;i++){
    const unsigned char*s=p; while(*s)s++; unsigned long el=(unsigned long)(s-p); s++;
    unsigned long co=uleb(&s);
    if(plen+(int)el<240){ for(unsigned long k=0;k<el;k++) prefix[plen+k]=(char)p[k];
      unsigned long cn=c->base+co;
      if(mm(cn)) twalk((const unsigned char*)cn,prefix,plen+(int)el,c); }
    p=s;
  }
}
// find image by dylib install-name among mapped Mach-O headers
static unsigned long find_image(const char*id, unsigned long*slide){
  for(unsigned long a=0x180000000UL; a<0x220000000UL; a+=0x4000){
    if(!ismo(a)) continue;
    struct mh64*h=(struct mh64*)a; unsigned long p=a+32;
    for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
      if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p;
        const char*name=(const char*)((unsigned long)d+d->nameoff);
        if(seq(name,id)){ if(slide)*slide=0; return a; } }
      if(l->cmdsize<8) break; p+=l->cmdsize;
    }
  }
  return 0;
}
static unsigned long img_trie(unsigned long hdr){
  struct mh64*h=(struct mh64*)hdr; unsigned long p=hdr+32, off=0;
  for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
    if(l->cmd==LC_DYLD_EXPORTS_TRIE){ struct ldc*d=(struct ldc*)p; off=d->dataoff; }
    if(!off && l->cmd==LC_DYLD_INFO_ONLY){ struct ldc*d=(struct ldc*)p; off=d->dataoff; }
    if(l->cmdsize<8) break; p+=l->cmdsize;
  }
  return off ? hdr+off : 0;
}
static unsigned long zsym(const char*image, const char*name){
  unsigned long hdr=find_image(image,0);
  if(!hdr) return 0;
  unsigned long trie=img_trie(hdr);
  if(!trie || !mm(trie)) return 0;
  struct tctx c; c.base=trie; c.want=name; c.wlen=0; while(name[c.wlen])c.wlen++;
  c.found=0; c.off=0; c.steps=0;
  char prefix[248];
  twalk((const unsigned char*)trie,prefix,0,&c);
  return c.found ? hdr + c.off : 0;
}
int main(int argc,char**argv,char**envp){
  // Verify against known answers from dlsym (recorded):
  //   getpid=0x188cde178 pthread_self=0x188d1d590 sysctl=0x188bb5944 mmap=0x188cde96c
  unsigned long k=find_image("/usr/lib/system/libsystem_kernel.dylib",0);
  o_lbl("kernel hdr="); o_puth(k); o_lbl(" trie="); o_puth(img_trie(k)); o_lbl("\n");
  o_lbl("getpid      = "); o_puth(zsym("/usr/lib/system/libsystem_kernel.dylib","getpid")); o_lbl("  expect 188cde178\n");
  o_lbl("mmap        = "); o_puth(zsym("/usr/lib/system/libsystem_kernel.dylib","mmap")); o_lbl("  expect 188cde96c\n");
  o_lbl("pthread_self= "); o_puth(zsym("/usr/lib/system/libsystem_pthread.dylib","pthread_self")); o_lbl("  expect 188d1d590\n");
  o_lbl("pthread_create= "); o_puth(zsym("/usr/lib/system/libsystem_pthread.dylib","pthread_create")); o_lbl("\n");
  o_lbl("sysctl      = "); o_puth(zsym("/usr/lib/system/libsystem_c.dylib","sysctl")); o_lbl("  expect 188bb5944\n");
  o_lbl("sysctlbyname= "); o_puth(zsym("/usr/lib/system/libsystem_c.dylib","sysctlbyname")); o_lbl("\n");
  o_lbl("dlsym       = "); o_puth(zsym("/usr/lib/system/libdyld.dylib","dlsym")); o_lbl("\n");
  return 0;
}
