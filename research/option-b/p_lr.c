#include "rs.h"
extern unsigned long ent_x30, ent_sp;
int main(int argc,char**argv,char**envp){
  o_lbl("x30(LR)="); o_puth(ent_x30); o_lbl(" sp="); o_puth(ent_sp); o_lbl("\n");
  o_lbl("argc="); o_putn(argc); o_lbl(" argv="); o_puth((unsigned long)argv); o_lbl(" envp="); o_puth((unsigned long)envp); o_lbl("\n");
  o_lbl("-- scan stack words [sp .. sp+0x8000] for ptrs in 0x180000000..0x200000000 --\n");
  unsigned long *s=(unsigned long*)ent_sp;
  int hits=0;
  for(unsigned long off=0; off<0x8000; off+=8){
    unsigned long w=s[off/8];
    if(w>=0x180000000UL && w<0x200000000UL){
      o_lbl("  sp+"); o_puth(off); o_lbl(" = "); o_puth(w); o_lbl("\n");
      if(++hits>40) break;
    }
  }
  o_lbl("hits="); o_putn(hits); o_lbl("\n");
  return 0;
}
