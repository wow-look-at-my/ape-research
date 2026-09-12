#include "rs.h"
int main(int argc,char**argv,char**envp){
  // select(93) with zero timeout == nanosleep substitute
  { long tv[2]={0,50000}; o_lbl("select(93,0,0,0,&tv)= "); o_putn(rs5(93,0,0,0,0,(long)tv)); o_lbl("\n"); }
  // sem_open raw 268
  { o_lbl("sem_open raw: "); }
  // thread_selfid(372)
  { o_lbl("thread_selfid(372)="); o_putn(rs0(372)); o_lbl("\n"); }
  // __semwait_signal(334)
  { o_lbl("__semwait_signal(334) 0arg="); o_putn(rs6(334,0,0,0,0,0,0)); o_lbl("\n"); }
  // mach traps
  { o_lbl("swtch_pri (mach trap 0x7500000? ) - test x16=-89:"); }
  return 0;
}
