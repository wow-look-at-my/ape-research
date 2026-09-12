#include "rs.h"
int main(int argc,char**argv,char**envp){
  int fds[2]={-7,-7};
  long r=rs1(42,(long)fds);
  o_lbl("pipe(42) r=");o_putn(r);o_lbl(" r=");o_putn(fds[0]);o_lbl(" w=");o_putn(fds[1]);o_lbl("\n");
  o_lbl("nanosleep(101)="); o_putn(rs2(101,0,0)); o_lbl("\n");
  return 0;
}
