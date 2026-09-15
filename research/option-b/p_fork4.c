#include "rs.h"
int main(int c,char**v,char**e){
  long orig=rs0(20);
  int fd[2]; rs1(42,(long)fd);
  long pid=rs0(2);
  long me=rs0(20);
  if(pid==0){ o_lbl("CHILD: forkrc==0! me="); o_putn(me); o_lbl("\n"); rs1(1,0); }
  if(me!=orig){ o_lbl("CHILD: forkrc!=0 me="); o_putn(me); o_lbl(" forkrc="); o_putn(pid); o_lbl("\n"); rs1(1,0); }
  o_lbl("PARENT: forkrc="); o_putn(pid); o_lbl(" me="); o_putn(me); o_lbl("\n");
  int st=0; long r=rs4(7,pid,(long)&st,0,0);
  o_lbl("PARENT wait4="); o_putn(r); o_lbl(" status="); o_puth(st); o_lbl(" wifexited="); o_putn((st&0x7f)==0); o_lbl(" code="); o_putn((st>>8)&0xff); o_lbl("\n");
  return 0;
}
