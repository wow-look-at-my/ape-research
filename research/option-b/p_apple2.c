#include "rs.h"
extern unsigned long _apple_vec;
int main(int argc,char**argv,char**envp){
  unsigned long *v=(unsigned long*)_apple_vec;
  o_lbl("apple_vec="); o_puth((unsigned long)v); o_lbl("\n");
  for(int i=0;i<16;i++){
    o_lbl("apple["); o_putn(i); o_lbl("]="); o_puth(v[i]);
    unsigned long p=v[i];
    if(p>0x1000 && p<0x0000ffffffffffffUL){
      // try to interpret as pointer to pointer or string
      unsigned long first=*(unsigned long*)p;
      o_lbl(" *="); o_puth(first);
      char*c=(char*)p; int printable=1;
      for(int k=0;k<8;k++){ if(c[k]<32||c[k]>126){printable=0;break;} }
      if(printable){ o_lbl(" str=\""); o_write(1,c,8); o_lbl("\""); }
    }
    o_lbl("\n");
  }
  return 0;
}
