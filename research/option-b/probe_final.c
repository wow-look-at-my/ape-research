#include "rs.h"
int main(int argc,char**argv,char**envp){
  o_lbl("== nanosleep(101) 0-arg ==\n"); o_lbl("  rc="); o_putn(rs1(101,0)); o_lbl(" (SIGSYS expected)\n"); o_lbl("UNREACHED\n");
  return 0;
}
