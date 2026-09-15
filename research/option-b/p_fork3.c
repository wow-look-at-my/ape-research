#include "rs.h"
int g=111;
int main(int c,char**v,char**e){
  int fds[2]; rs1(42,(long)fds);
  long pid=rs0(2);
  if(pid==0||pid==rs0(20)){
    // ambiguous: use getppid to decide
  }
  long ppid=rs0(39);
  o_lbl("proc pid=");o_putn(rs0(20));o_lbl(" ppid=");o_putn(ppid);o_lbl(" forkrc=");o_putn(pid);o_lbl(" g=");o_putn(g);o_lbl("\n");
  g=222;
  if(ppid!=1 && rs0(39)!=0){ /* nothing */ }
  return 0;
}
