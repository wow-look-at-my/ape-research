#include "rs.h"
int main(int c,char**v,char**e){ o_lbl("start\n"); for(volatile int i=0;i<3;i++){}
  o_lbl("before pipe\n");
  int fds[2]={-7,-7};
  long r=rs1(42,(long)fds);
  o_lbl("pipe rc=");o_putn(r);o_lbl(" a=");o_putn(fds[0]);o_lbl(" b=");o_putn(fds[1]);o_lbl("\n");
  o_lbl("after pipe\n"); return 0; }
