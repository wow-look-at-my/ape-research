#include "rs.h"
// ================= zero-import symbol resolver (final) =================
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct seg64 { unsigned int cmd,cmdsize; char segname[16]; unsigned long vmaddr,vmsize,fileoff,filesize; int maxprot,initprot; unsigned int nsects,flags; };
struct dylibc { unsigned int cmd,cmdsize; unsigned int nameoff,timestamp,curver,compatver; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
#define LC_ID_DYLIB 0xd
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
  struct mh64*h=(struct mh64*)p; return h->ncmds>0&&h->ncmds<8192&&h->sizeofcmds>0&&h->sizeofcmds<0x40000; }
// Is this a Mach-O header that begins the __TEXT page of an image? i.e. the
// filetype is a dylib/executable and the first segment is __TEXT with vmaddr page-aligned.
static int is_image_start(unsigned long p,unsigned long*textvm){
  if(!ismo(p)) return 0;
  struct mh64*h=(struct mh64*)p; if(h->filetype!=6 && h->filetype!=2 && h->filetype!=8 && h->filetype!=0xb) return 0;
  unsigned long q=p+32;
  for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)q;
    if(l->cmd==0x19){ struct seg64*s=(struct seg64*)q;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='T'&&s->segname[3]=='E'&&s->segname[4]=='X'&&s->segname[5]=='T'){
        if(textvm)*textvm=s->vmaddr; return 1; }
      return 0; }
    if(l->cmdsize<8) break; q+=l->cmdsize; }
  return 0;
}
// Build the shared-cache image list by scanning for image starts once, then
// match names via LC_ID_DYLIB.
static unsigned long g_hdr[512]; static int g_n;
static void collect(void){
  g_n=0;
  for(unsigned long a=0x180000000UL; a<0x260000000UL && g_n<512; a+=0x4000){
    unsigned long tv;
    if(is_image_start(a,&tv)) g_hdr[g_n++]=a;
  }
}
static unsigned long find_image(const char*want){
  for(int i=0;i<g_n;i++){
    unsigned long a=g_hdr[i]; struct mh64*h=(struct mh64*)a; unsigned long p=a+32;
    for(unsigned int c=0;c<h->ncmds;c++){ struct lc*l=(struct lc*)p;
      if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p; unsigned long np=(unsigned long)d+d->nameoff;
        if(mapped(np)&&seq((const char*)np,want)) return a; }
      if(l->cmdsize<8||l->cmdsize>0x20000) break; p+=l->cmdsize; }
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
  if(!mapped(imghdr)) return 0;
  struct mh64*h=(struct mh64*)imghdr; unsigned long p=imghdr+32,dataoff=0;
  for(unsigned int i=0;i<h->ncmds && i<4096;i++){ struct lc*l=(struct lc*)p;
    if(l->cmd==LC_DYLD_EXPORTS_TRIE||l->cmd==LC_DYLD_INFO_ONLY){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; break; }
    if(l->cmdsize<8) break; p+=l->cmdsize; }
  if(!dataoff) return 0;
  unsigned long trie=imghdr+dataoff; if(!mapped(trie)) return 0;
  char pfx[260]; unsigned long out=0; long budget=3000000;
  if(walk(trie,pfx,0,sym,slen(sym),trie,imghdr,&out,&budget)) return out;
  return 0;
}
static void puth(unsigned long v){o_lbl("0x");o_puth(v);}
int main(int argc,char**argv,char**envp){
  o_lbl("== zero-import resolver (final) ==\n");
  collect();
  o_lbl("images found: "); o_putn(g_n); o_lbl("\n");
  struct { const char*img; const char*sym; } T[]={
    {"/usr/lib/system/libsystem_kernel.dylib","_getpid"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mmap"},
    {"/usr/lib/system/libsystem_kernel.dylib","_mach_vm_region"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_self"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_create"},
    {"/usr/lib/system/libsystem_pthread.dylib","_pthread_kill"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctl"},
    {"/usr/lib/system/libsystem_c.dylib","_sysctlbyname"},
    {"/usr/lib/system/libdyld.dylib","_dlsym"},
    {"/usr/lib/system/libdyld.dylib","_dlopen"},
    {"/usr/lib/system/libsystem_platform.dylib","_sys_icache_invalidate"},
  };
  for(unsigned i=0;i<sizeof(T)/sizeof(T[0]);i++){
    unsigned long h=find_image(T[i].img);
    unsigned long r=h?resolve(h,T[i].sym):0;
    o_lbl(T[i].sym); o_lbl(" = "); puth(r);
    if(!h) o_lbl("  (image not found)");
    o_lbl("\n");
  }
  return 0;
}
