#include "rs.h"
int main(int c,char**v,char**e){
  o_lbl("before fork\n");
  long pid=rs0(2);
  o_lbl("fork rc="); o_putn(pid); o_lbl("\n");
  if(pid==0){ o_lbl("CHILD pid(20)="); o_putn(rs0(20)); o_lbl("\n"); rs1(1,0); }
  { int st=0; long r=rs4(7,pid,(long)&st,0,0);
    o_lbl("wait4="); o_putn(r); o_lbl(" st="); o_puth(st); o_lbl("\n"); }
  return 0;
}
