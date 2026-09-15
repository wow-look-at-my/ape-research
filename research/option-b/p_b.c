#include "rs.h"
int main(int c,char**v,char**e){ o_lbl("start\n");
  o_lbl("before nanosleep\n");
  long r=rs2(101,0,0);
  o_lbl("nanosleep rc=");o_putn(r);o_lbl("\n");
  o_lbl("after nanosleep\n"); return 0; }
