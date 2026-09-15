#include "rs.h"
/* Zero-import binary that also weakly references a hedge dylib. After
 * libSystem's name is changed (so it is never loaded), resolve malloc and
 * try to use it. Tests whether "symbols resolvable" implies "libc usable". */
static int mapped(unsigned long a){ a&=~0x3fffUL; unsigned char v[2]; v[0]=0;
  if(rs3(78,a,16384,(long)v)!=0) return 0; return !(v[0]&0x80); }
static int rdb(unsigned long a,unsigned char*o){ if(!mapped(a))return -1; *o=*(volatile unsigned char*)a; return 0; }
static int uleb(unsigned long*a,unsigned long*v){ unsigned long r=0;int s=0;
  for(int i=0;i<10;i++){unsigned char b; if(rdb(*a,&b))return -1; (*a)++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80)){*v=r;return 0;} s+=7;} return -1; }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static int ismo(unsigned long p){ if(!mapped(p))return 0; if(*(volatile unsigned int*)p!=0xfeedfacf)return 0;
  return *(volatile unsigned int*)(p+16)>0 && *(volatile unsigned int*)(p+16)<2048; }
static long g_budget;
static int walk(unsigned long n,char*pfx,int pl,const char*w,int wl,unsigned long t,unsigned long h,unsigned long*o){
  if(g_budget<=0||pl>250||!mapped(n))return 0; g_budget--;
  unsigned long a=n,term,fl; if(uleb(&a,&term))return 0;
  if(term){ if(uleb(&a,&fl))return 0;
    if(fl&0x08){ unsigned long x; if(uleb(&a,&x))return 0; unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++; a=s+1; }
    else { unsigned long off; if(uleb(&a,&off))return 0; if(pl==wl){pfx[pl]=0; if(seq(pfx,w)){*o=h+off;return 1;}} } }
  unsigned char nc; if(rdb(a,&nc))return 0; a++;
  for(unsigned k=0;k<nc;k++){ unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++;
    int el=(int)(s-a); s++; unsigned long co; if(uleb(&s,&co))return 0;
    if(pl+el<250&&mapped(a)&&mapped(a+el-1)){
      /* prefix pruning: the edge must continue the wanted name */
      int ok=1;
      for(int i=0;i<el;i++){ char ch=(char)*(volatile unsigned char*)(a+i); pfx[pl+i]=ch;
        if(pl+i>=wl||ch!=w[pl+i]){ ok=0; break; } }
      if(ok && walk(t+co,pfx,pl+el,w,wl,t,h,o))return 1; }
    a=s; }
  return 0; }
static unsigned long lookup_all(const char*sym){
  /* Find the cache and search every image's export trie. */
  unsigned long cb=0;
  for(unsigned long a=0x180000000UL;a<0x1c0000000UL;a+=0x4000){ if(mapped(a)&&*(volatile unsigned int*)a==0x646c7964){cb=a;break;} }
  if(!cb)return 0;
  unsigned char*b=(unsigned char*)cb;
  unsigned long ito=*(volatile unsigned long*)(b+0x88), itc=*(volatile unsigned long*)(b+0x90);
  if(ito<0x100||ito>0x100000||itc<50||itc>20000)return 0;
  unsigned long slide=cb-0x180000000UL;
  for(unsigned long i=0;i<itc;i++){
    unsigned long h=*(volatile unsigned long*)(b+ito+i*32+16)+slide;
    if(!ismo(h))continue;
    unsigned long p=h+32,tv=0,doff=0,ldvm=0,ldfo=0;
    unsigned int ncmds=*(volatile unsigned int*)(h+16);
    for(unsigned int c=0;c<ncmds&&c<4096;c++){ if(!mapped(p))break; unsigned int cmd=*(volatile unsigned int*)p, sz=*(volatile unsigned int*)(p+4);
      if(cmd==0x19){ if(*(char*)(p+8)=='_'&&*(char*)(p+12)=='X') tv=*(volatile unsigned long*)(p+24);
        if(*(char*)(p+8)=='_'&&*(char*)(p+10)=='L'){ ldvm=*(volatile unsigned long*)(p+24); ldfo=*(volatile unsigned long*)(p+32);} }
      if((cmd==0x80000033||cmd==0x80000022)&&!doff) doff=*(volatile unsigned int*)(p+8);
      if(sz<8)break; p+=sz; }
    if(!tv||!doff)continue;
    unsigned long t=ldvm+(h-tv)+(doff-ldfo); if(!mapped(t))continue;
    char pfx[260]; unsigned long o=0; g_budget=2000000;
    if(walk(t,pfx,0,sym,slen(sym),t,h,&o)) return o;
  }
  return 0; }
static void puth(unsigned long v){o_lbl("0x");o_puth(v);}
int main(int argc,char**argv,char**envp){
  o_lbl("=== hedge-rescued process, libSystem absent ===\n");
  unsigned long mp=lookup_all("_malloc");
  unsigned long gpu=lookup_all("_getpid");
  o_lbl("malloc  = "); puth(mp); o_lbl("\n");
  o_lbl("getpid  = "); puth(gpu); o_lbl("\n");
  if(gpu){ long(*g)(void)=(long(*)(void))gpu; o_lbl("getpid() = "); o_putn(g()); o_lbl("  (pure code, no libc state)\n"); }
  if(mp){ void*(*m)(unsigned long)=(void*(*)(unsigned long))mp;
    o_lbl("calling malloc(64)...\n"); void*p=m(64);
    o_lbl("  malloc returned "); puth((unsigned long)p); o_lbl("\n"); }
  else o_lbl("malloc not found\n");
  return 0;
}
