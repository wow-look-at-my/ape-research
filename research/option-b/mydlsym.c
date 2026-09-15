#include "rs.h"
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct seg64 { unsigned int cmd,cmdsize; char segname[16]; unsigned long vmaddr,vmsize,fileoff,filesize; int maxprot,initprot; unsigned int nsects,flags; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
#define LC_SEGMENT_64 0x19
static int mapped(unsigned long a){ a&=~0x3fffUL; unsigned char v[2]; v[0]=0;
  if(rs3(78,a,16384,(long)v)!=0) return 0; return !(v[0]&0x80); }
static int rdb(unsigned long a,unsigned char*o){ if(!mapped(a))return -1; *o=*(volatile unsigned char*)a; return 0; }
static int uleb(unsigned long*a,unsigned long*v){ unsigned long r=0;int s=0;
  for(int i=0;i<10;i++){unsigned char b; if(rdb(*a,&b))return -1; (*a)++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80)){*v=r;return 0;} s+=7;} return -1; }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static int ismo(unsigned long p){ if(!mapped(p))return 0; if(*(volatile unsigned int*)p!=0xfeedfacf)return 0;
  struct mh64*h=(struct mh64*)p; return h->ncmds>0&&h->ncmds<2048&&h->sizeofcmds>32&&h->sizeofcmds<0x40000; }
static unsigned long cache_base(void){ for(unsigned long a=0x180000000UL;a<0x1c0000000UL;a+=0x4000){
  if(mapped(a)&&*(volatile unsigned int*)a==0x646c7964) return a; } return 0; }
static int parse(unsigned long h,unsigned long*doff,unsigned long*ldvm,unsigned long*ldfo,unsigned long*slid){
  struct mh64*mh=(struct mh64*)h; unsigned long p=h+32; unsigned long tv=0; *doff=0;*ldvm=0;*ldfo=0;
  for(unsigned int i=0;i<mh->ncmds&&i<4096;i++){ if(!mapped(p)||!mapped(p+8))return 0; struct lc*l=(struct lc*)p;
    if(l->cmd==LC_SEGMENT_64){ struct seg64*s=(struct seg64*)p;
      if(s->segname[0]=='_'&&s->segname[4]=='X'&&s->segname[5]=='T') tv=s->vmaddr;
      if(s->segname[0]=='_'&&s->segname[2]=='L'){*ldvm=s->vmaddr;*ldfo=s->fileoff;} }
    if((l->cmd==0x80000033||l->cmd==0x80000022)&&!*doff){ struct ldc*d=(struct ldc*)p; *doff=d->dataoff; }
    if(l->cmdsize<8||l->cmdsize>0x40000)break; p+=l->cmdsize; }
  if(!tv||!*doff) return 0; *slid=h-tv; return 1; }
static long g_budget;
static int walk(unsigned long node,char*pfx,int plen,const char*want,int wlen,unsigned long trie,unsigned long imghdr,unsigned long*out){
  if(g_budget<=0||plen>250||!mapped(node))return 0; g_budget--;
  unsigned long a=node,term,fl; if(uleb(&a,&term))return 0;
  if(term){ if(uleb(&a,&fl))return 0;
    if(fl&0x08){ unsigned long o; if(uleb(&a,&o))return 0; unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++; a=s+1; }
    else { unsigned long off; if(uleb(&a,&off))return 0;
      if(plen==wlen){ pfx[plen]=0; if(seq(pfx,want)){*out=imghdr+off;return 1;} } } }
  unsigned char nc; if(rdb(a,&nc))return 0; a++;
  for(unsigned k=0;k<nc;k++){ unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++;
    int el=(int)(s-a); s++; unsigned long co; if(uleb(&s,&co))return 0;
    if(plen+el<250&&mapped(a)&&mapped(a+el-1)){ for(int i=0;i<el;i++) pfx[plen+i]=(char)*(volatile unsigned char*)(a+i);
      if(walk(trie+co,pfx,plen+el,want,wlen,trie,imghdr,out))return 1; } a=s; }
  return 0; }
static unsigned long res_one(unsigned long h,const char*sym){
  unsigned long doff,ldvm,ldfo,slid; if(!parse(h,&doff,&ldvm,&ldfo,&slid))return 0;
  unsigned long trie=ldvm+slid+(doff-ldfo); if(!mapped(trie))return 0;
  char pfx[260]; unsigned long out=0; g_budget=2000000;
  if(walk(trie,pfx,0,sym,slen(sym),trie,h,&out))return out; return 0; }
/* ---- drop-in dlsym replacement: handle ignored, searches the whole cache ---- */
static void *my_dlsym(void*handle,const char*name){
  (void)handle;
  if(name[0]!='_'){ /* cache exports are all underscore-prefixed; try both */ }
  unsigned long cb=cache_base(); if(!cb)return 0;
  unsigned char*b=(unsigned char*)cb;
  unsigned long ito=*(volatile unsigned long*)(b+0x88), itc=*(volatile unsigned long*)(b+0x90);
  if(ito<0x100||ito>0x100000||itc<50||itc>20000)return 0;
  unsigned long slide=cb-0x180000000UL;
  /* first try a fast pass over images; resolve_primary for the name with and without _ */
  for(unsigned long i=0;i<itc;i++){
    unsigned long h=*(volatile unsigned long*)(b+ito+i*32+16)+slide;
    if(!ismo(h))continue;
    unsigned long r=res_one(h,name);
    if(!r && name[0]!='_'){ /* try underscore prefix */ char t[128]; t[0]='_'; int k=0; while(name[k]&&k<126){t[k+1]=name[k];k++;} t[k+1]=0; r=res_one(h,t); }
    if(r)return (void*)r;
  }
  return 0; }
static void puth(unsigned long v){o_lbl("0x");o_puth(v);}
int main(int argc,char**argv,char**envp){
  o_lbl("== my_dlsym (zero-import dlsym replacement) ==\n");
  const char*names[]={"getpid","_getpid","malloc","pthread_mutex_lock","pthread_cond_wait","pthread_cond_timedwait_relative_np","mmap","sysctl","dlsym","mach_vm_region",0};
  for(int i=0;names[i];i++){
    void*p=my_dlsym(0,names[i]);
    o_lbl(names[i]); o_lbl(" = "); puth((unsigned long)p); o_lbl("\n");
  }
  o_lbl("calling my_dlsym(\"getpid\")()...\n");
  long (*gp)(void)=(long(*)(void))my_dlsym(0,"getpid");
  if(gp){ o_lbl("  pid="); o_putn(gp()); o_lbl("\n"); }
  o_lbl("calling my_dlsym(\"pthread_self\")()...\n");
  unsigned long (*ps)(void)=(unsigned long(*)(void))my_dlsym(0,"pthread_self");
  if(ps){ o_lbl("  self="); puth(ps()); o_lbl("\n"); }
  return 0;
}
