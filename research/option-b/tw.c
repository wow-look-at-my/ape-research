#include "rs.h"
static int mm(unsigned long a){ a&=~0x3fffUL; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
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
static unsigned long lookup(unsigned long trie,const char*name){
  int n=0; while(name[n])n++;
  struct tctx c={trie,name,n,0,0,0};
  char prefix[248];
  twalk((const unsigned char*)trie,prefix,0,&c);
  return c.found?trie+c.off:0;
}
int main(int argc,char**argv,char**envp){
  // ground truth: kernel hdr 0x188cdd000, trie at hdr+0xb686d0
  unsigned long k=0x188cdd000UL, kt=k+0xb686d0UL;
  o_lbl("kernel trie="); o_puth(kt); o_lbl(" first bytes: ");
  { unsigned char*b=(unsigned char*)kt; for(int i=0;i<8;i++){ o_puth(b[i]); o_lbl(" "); } }
  o_lbl("\n");
  o_lbl("getpid -> "); o_puth(lookup(kt,"getpid")); o_lbl("  (expect 0x188cde178, hdr+0xf34)\n");
  o_lbl("mmap   -> "); o_puth(lookup(kt,"mmap")); o_lbl("  (expect 0x188cde96c, hdr+0x1958?)\n");
  unsigned long pt=0x188d1b000UL+0xb71498UL;
  o_lbl("pthread trie="); o_puth(pt); o_lbl("\n");
  o_lbl("pthread_self -> "); o_puth(lookup(pt,"pthread_self")); o_lbl("  (expect 0x188d1d590)\n");
  unsigned long ct=0x188bb0000UL+0xb4dc50UL;
  o_lbl("sysctl -> "); o_puth(lookup(ct,"sysctl")); o_lbl("  (expect 0x188bb5944)\n");
  return 0;
}
