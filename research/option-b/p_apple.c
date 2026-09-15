#include "rs.h"
typedef int (*main4_t)(int,char**,char**,void**);
int main(int argc,char**argv,char**envp) __asm__("_main");
int main(int argc,char**argv,char**envp){
  // The 4th argument dyld passes to main is NOT directly accessible in C,
  // but it is in x3 at entry. Capture it with a naked trampoline is hard in C;
  // instead scan the stack above argv for the apple vector by known shape.
  o_lbl("argc="); o_putn(argc); o_lbl("\n");
  for(int i=0;i<argc && i<4;i++){ o_lbl("argv["); o_putn(i); o_lbl("]="); o_puts(argv[i]); o_lbl("\n"); }
  // envp end
  int e=0; while(envp[e])e++;
  o_lbl("envc="); o_putn(e); o_lbl(" envp_end="); o_puth((unsigned long)&envp[e]); o_lbl(" argv="); o_puth((unsigned long)argv);
  o_lbl(" &argv="); o_puth((unsigned long)&argv); o_lbl("\n");
  // Dump 40 words after envp[e]+1 (auxv + apple vector region)
  unsigned long *p=(unsigned long*)(&envp[e])+1;
  for(int i=0;i<40;i++){ o_lbl("w["); o_putn(i); o_lbl("]="); o_puth(p[i]); 
    if(p[i]>0x100000000UL && p[i]<0x800000000000UL){ 
      // try to read as pointer/string
      unsigned long long *q=(unsigned long long*)p[i]; (void)q;
    }
    o_lbl("\n"); }
  return 0;
}
