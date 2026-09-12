#include "rs.h"
// Capture x0 (fork's raw return) and getpid() immediately, in both processes.
__attribute__((noinline)) static long fk(void){ return rs0(2); }
int main(int argc,char**argv,char**envp){
  long p0=rs0(20);
  long x0=fk();
  long p1=rs0(20);
  long pp=rs0(39);
  o_lbl("p0="); o_putn(p0); o_lbl(" p1="); o_putn(p1); o_lbl(" ppid="); o_putn(pp);
  o_lbl(" fork_x0="); o_putn(x0);
  o_lbl(" is_child="); o_putn(p1!=p0);
  o_lbl("\n");
  return 0;
}
