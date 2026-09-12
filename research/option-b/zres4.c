#include "rs.h"
struct mh64 { unsigned int magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { unsigned int cmd,cmdsize; };
struct ldc { unsigned int cmd,cmdsize,dataoff,datasize; };
static int mapped(unsigned long a){ a&=~0x3fffUL; unsigned char v[2]; v[0]=0;
  if(rs3(78,a,16384,(long)v)!=0) return 0; return !(v[0]&0x80); }
static int rdb(unsigned long a,unsigned char*o){ if(!mapped(a))return -1; *o=*(volatile unsigned char*)a; return 0; }
static int uleb(unsigned long*a,unsigned long*v){ unsigned long r=0;int s=0;
  for(int i=0;i<10;i++){unsigned char b; if(rdb(*a,&b))return -1; (*a)++; r|=(unsigned long)(b&0x7f)<<s; if(!(b&0x80)){*v=r;return 0;} s+=7;} return -1; }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static long g_budget=2000000; static long g_nodes=0;
static int walk(unsigned long node,char*pfx,int plen,const char*want,int wlen,unsigned long trie,unsigned long*out){
  if(g_budget<=0) return 0;
  if(plen>250) return 0;
  if(!mapped(node)) return 0;
  g_budget--; g_nodes++;
  unsigned long a=node,term,flags;
  if(uleb(&a,&term)) return 0;
  if(term){
    if(uleb(&a,&flags)) return 0;
    if(flags&0x08){ unsigned long ord; if(uleb(&a,&ord))return 0; unsigned long s=a; unsigned char b;
      while(!rdb(s,&b)&&b)s++; a=s+1; }
    else { unsigned long off; if(uleb(&a,&off)) return 0;
      if(plen==wlen&&!seq(pfx,want)){ *out=off; return 1; } } }
  unsigned char nc; if(rdb(a,&nc)) return 0; a++;
  for(unsigned k=0;k<nc;k++){
    unsigned long s=a; unsigned char b; while(!rdb(s,&b)&&b)s++;
    int el=(int)(s-a); s++;
    unsigned long co; if(uleb(&s,&co)) return 0;
    if(plen+el<250 && mapped(a) && mapped(a+el-1)){
      for(int i=0;i<el;i++) pfx[plen+i]=(char)*(volatile unsigned char*)(a+i);
      if(walk(trie+co,pfx,plen+el,want,wlen,trie,out)) return 1;
    }
    a=s;
  }
  return 0;
}
int main(int c,char**v,char**e){
  // libsystem_kernel hdr 0x188cdd000, dataoff 0xb686d0 => trie 0x1898456d0
  unsigned long imghdr=0x188cdd000UL;
  struct mh64*h=(struct mh64*)imghdr; unsigned long p=imghdr+32,dataoff=0;
  for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
    if(l->cmd==0x80000033||l->cmd==0x80000022){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; break; }
    p+=l->cmdsize; }
  unsigned long trie=imghdr+dataoff;
  o_lbl("dataoff="); o_puth(dataoff); o_lbl(" trie="); o_puth(trie); o_lbl(" mapped="); o_putn(mapped(trie)); o_lbl("\n");
  o_lbl("root bytes: "); for(int i=0;i<16;i++){ o_puth(*(volatile unsigned char*)(trie+i)); o_lbl(" "); } o_lbl("\n");
  char pfx[260]; unsigned long off=0;
  int r=walk(trie,pfx,0,"_getpid",7,trie,&off);
  o_lbl("found="); o_putn(r); o_lbl(" off="); o_puth(off); o_lbl(" nodes="); o_putn(g_nodes);
  if(r){ o_lbl(" => "); o_puth(imghdr+off); }
  o_lbl("\n");
  return 0;
}
