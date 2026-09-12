#include "rs.h"
// =====================================================================
// Option B zero-import bootstrap: locate libSystem's Mach-O image and
// resolve any exported symbol through its dyld export trie. No imports,
// no dlsym, no LC_SYMTAB fixups. Only raw BSD syscalls (mincore/read/...).
// =====================================================================
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct seg64 { unsigned int cmd,cmdsize; char segname[16]; unsigned long vmaddr,vmsize,fileoff,filesize; int maxprot,initprot; unsigned int nsects,flags; };
struct dylibc { unsigned int cmd,cmdsize; unsigned int nameoff,timestamp,curver,compatver; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
#define LC_ID_DYLIB 0xd
#define LC_DYLD_EXPORTS_TRIE 0x80000033
#define LC_DYLD_INFO_ONLY 0x80000022

// mincore: vec[0] bit 7 (0x80) set => page is NOT mapped (per XNU).
static int mapped(unsigned long a){
  a &= ~0x3fffUL;
  unsigned char vec[2]; vec[0]=0;
  if(rs3(78,a,16384,(long)vec)!=0) return 0;
  return !(vec[0]&0x80);
}
static int rdb(unsigned long a,unsigned char*o){ if(!mapped(a)) return -1; *o=*(volatile unsigned char*)a; return 0; }
static int uleb(unsigned long*a,unsigned long*v){
  unsigned long r=0; int s=0;
  for(int i=0;i<10;i++){ unsigned char b; if(rdb(*a,&b)) return -1; (*a)++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80)){*v=r;return 0;} s+=7; }
  return -1;
}
static int seq(const char*a,const char*b){ while(*a&&*a==*b){a++;b++;} return *a==*b; }
static int slen(const char*s){int n=0;while(s[n])n++;return n;}

// Walk an export trie rooted at 'trie'; returns true and sets *out to the
// resolved symbol's address (image_header + export offset) on success.
static int walk(unsigned long node,char*pfx,int plen,const char*want,int wlen,
                unsigned long trie,unsigned long imghdr,unsigned long*out,long*budget){
  if(*budget<=0) return 0;
  if(!mapped(node)) return 0;
  unsigned long a=node;
  unsigned long term; if(uleb(&a,&term)) return 0;
  (*budget)--;
  if(term){
    unsigned long flags; if(uleb(&a,&flags)) return 0;
    if(flags&0x08){ // reexport: skip ordinal + imported name
      unsigned long ord; if(uleb(&a,&ord)) return 0;
      unsigned long s=a; unsigned char b;
      while(!rdb(s,&b)&&b) s++;
      a=s+1;
    } else {
      unsigned long off; if(uleb(&a,&off)) return 0;
      if(plen==wlen && !seq(pfx,want)){ *out=imghdr+off; return 1; }
    }
  }
  unsigned char nc; if(rdb(a,&nc)) return 0; a++;
  for(unsigned k=0;k<nc;k++){
    unsigned long s=a; unsigned char b;
    while(!rdb(s,&b)&&b) s++;
    int el=(int)(s-a); s++;
    unsigned long co; if(uleb(&s,&co)) return 0;
    if(plen>0 && plen+el>=plen && plen+el<256){
      if(mapped(a)&&mapped(a+el-1)){
        for(int i=0;i<el;i++) pfx[plen+i]=(char)*(volatile unsigned char*)(a+i);
        if(walk(trie+co,pfx,plen+el,want,wlen,trie,imghdr,out,budget)) return 1;
      }
    }
    a=s;
  }
  return 0;
}
static unsigned long lookup(unsigned long imghdr,const char*sym){
  struct mh64*h=(struct mh64*)imghdr;
  if(!mapped(imghdr) || h->magic!=0xfeedfacf) return 0;
  unsigned long p=imghdr+32, dataoff=0;
  for(unsigned int i=0;i<h->ncmds && i<4096;i++){
    struct lc*l=(struct lc*)p;
    if(l->cmd==LC_DYLD_EXPORTS_TRIE){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; break; }
    if(l->cmd==LC_DYLD_INFO_ONLY){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; break; }
    if(l->cmdsize<8) break;
    p+=l->cmdsize;
  }
  if(!dataoff) return 0;
  unsigned long trie=imghdr+dataoff;
  if(!mapped(trie)) return 0;
  char pfx[260]; unsigned long out=0; long budget=2000000;
  if(walk(trie,pfx,0,sym,slen(sym),trie,imghdr,&out,&budget)) return out;
  return 0;
}
// Find an image by its LC_ID_DYLIB install name, scanning mapped cache pages.
static unsigned long find_image(const char*want){
  for(unsigned long a=0x180000000UL; a<0x260000000UL; a+=0x4000){
    if(!mapped(a)) continue;
    if(*(volatile unsigned int*)a!=0xfeedfacf) continue;
    struct mh64*h=(struct mh64*)a;
    if(h->ncmds==0||h->ncmds>8192||h->sizeofcmds==0||h->sizeofcmds>0x20000) continue;
    unsigned long p=a+32;
    for(unsigned int i=0;i<h->ncmds;i++){
      struct lc*l=(struct lc*)p;
      if(l->cmd==LC_ID_DYLIB){
        struct dylibc*d=(struct dylibc*)p;
        unsigned long np=(unsigned long)d+d->nameoff;
        if(mapped(np)&&seq((const char*)np,want)) return a;
      }
      if(l->cmdsize<8||l->cmdsize>0x10000) break;
      p+=l->cmdsize;
    }
  }
  return 0;
}
static void puth(unsigned long v){ o_lbl("0x"); o_puth(v); }
int main(int argc,char**argv,char**envp){
  o_lbl("== zero-import resolver ==\n");
  const char*imgs[]={"/usr/lib/system/libsystem_kernel.dylib",
                     "/usr/lib/system/libsystem_pthread.dylib",
                     "/usr/lib/system/libsystem_c.dylib",
                     "/usr/lib/system/libdyld.dylib",0};
  for(int i=0;imgs[i];i++){
    unsigned long h=find_image(imgs[i]);
    o_lbl("img "); o_puts(imgs[i]); o_lbl(" -> "); puth(h); o_lbl("\n");
  }
  unsigned long kh=find_image("/usr/lib/system/libsystem_kernel.dylib");
  if(kh){
    o_lbl("kernel hdr="); puth(kh); o_lbl("\n");
    unsigned long r=lookup(kh,"_getpid");  o_lbl("  _getpid       = "); puth(r); o_lbl("\n");
    r=lookup(kh,"_mmap");                  o_lbl("  _mmap         = "); puth(r); o_lbl("\n");
    r=lookup(kh,"_mach_vm_region");        o_lbl("  _mach_vm_region = "); puth(r); o_lbl("\n");
    o_lbl("calling resolved _getpid()...\n");
    long (*gp)(void)=(long(*)(void))lookup(kh,"_getpid");
    if(gp){ o_lbl("  getpid()="); o_putn(gp()); o_lbl("\n"); }
  }
  return 0;
}
