#include "rs.h"
int main(int c,char**v,char**e){
  unsigned long probes[]={0x100000000UL,0x180000000UL,0x188830000UL,0x188944000UL,0x188963000UL,0xdead0000UL,0x1000000000UL,0};
  for(int i=0;probes[i];i++){
    char vec[64]; for(int k=0;k<64;k++)vec[k]=0x7f;
    long r=rs3(78,probes[i],16384,(long)vec);
    o_lbl("mincore("); o_puth(probes[i]); o_lbl(") rc="); o_putn(r); o_lbl(" vec[0]="); o_puth((unsigned char)vec[0]); o_lbl("\n");
  }
  // Try probing a page in a known image and walking down
  o_lbl("walk down from 0x188963000 in 16K steps:\n");
  for(unsigned long a=0x188963000UL; a>0x188900000UL; a-=0x4000){
    char vec[4]; vec[0]=0x7f;
    long r=rs3(78,a,16384,(long)vec);
    if(r!=0 || (unsigned char)vec[0]==0x80){
      o_lbl("  unmapped at "); o_puth(a); o_lbl(" (rc="); o_putn(r); o_lbl(" vec0="); o_puth((unsigned char)vec[0]); o_lbl(")\n");
      break;
    }
    unsigned int magic=*(volatile unsigned int*)a;
    if(magic==0xfeedfacf){ o_lbl("  MACHO at "); o_puth(a); o_lbl("\n"); }
  }
  return 0;
}
