#include "rs.h"
// argv[1] = classnum (decimal, may be large). Call with 0 args, print rc.
static unsigned long parse(const char*s){unsigned long v=0;while(*s)v=v*10+(*s++-'0');return v;}
int main(int argc,char**argv,char**envp){
  unsigned long cn=parse(argv[1]);
  long r=rs_syscall6((long)cn,0,0,0,0,0,0);
  o_lbl("classnum="); o_puth(cn); o_lbl(" rc="); o_putn(r); o_lbl("\n");
  return 0;
}
