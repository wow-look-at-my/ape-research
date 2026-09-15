#include "rs.h"
static int mm(unsigned long a){ if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
int main(int argc,char**argv,char**envp){
  o_lbl("scanning for dyld cache magic 'dyld_v1'...\n");
  for(unsigned long a=0x170000000UL; a<0x1a0000000UL; a+=0x4000){
    if(!mm(a)) continue;
    unsigned int m=*(volatile unsigned int*)a;
    // 'dyld' = 0x646c7964 little-endian
    if(m==0x646c7964){
      char*b=(char*)a;
      o_lbl("magic @"); o_puth(a); o_lbl(": "); 
      for(int i=0;i<32;i++){ o_puth((unsigned char)b[i]); o_lbl(" "); }
      o_lbl("\n  ascii: "); o_write(1,b,32); o_lbl("\n");
      break;
    }
  }
  return 0;
}
