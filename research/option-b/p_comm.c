#include "rs.h"
int main(int argc,char**argv,char**envp){
  unsigned long base=0x0000000FFFFFC000UL;
  unsigned int *cp=(unsigned int*)base;
  o_lbl("commpage magic words:\n");
  for(int i=0;i<24;i++){ o_lbl("  +"); o_puth((unsigned long)(i*4)); o_lbl("="); o_puth(cp[i]); o_lbl("\n"); }
  o_lbl("scan commpage for pointers into 0x180000000-0x1c0000000 (cache) or 0x100000000+ :\n");
  unsigned long *q=(unsigned long*)base;
  for(int i=0;i<1024;i++){
    unsigned long w=q[i];
    if(w>0x180000000UL && w<0x1c0000000UL){ o_lbl("  +"); o_puth((unsigned long)(i*8)); o_lbl("="); o_puth(w); o_lbl("\n"); }
  }
  o_lbl("done\n");
  return 0;
}
