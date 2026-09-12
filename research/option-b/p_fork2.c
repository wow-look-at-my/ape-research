#include "rs.h"
static void out(const char*tag,long a,long b,long c){ o_lbl(tag);o_putn(a);o_lbl(",");o_putn(b);o_lbl(",");o_putn(c);o_lbl("\n"); }
int main(int c,char**v,char**e){
  long me=rs0(20);
  o_lbl("A pid="); o_putn(me); o_lbl("\n");
  long pid=rs0(2);
  long me2=rs0(20);
  o_lbl("B pid="); o_putn(me2); o_lbl(" forkrc="); o_putn(pid); o_lbl(pid==0?" [CHILD]\n":" [PARENT]\n");
  if(pid==0){ out("C child pid=",rs0(20),0,0); rs1(1,0); }
  out("C parent pid=",me2,pid,0);
  return 0;
}
