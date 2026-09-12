#include "rs.h"
int main(int c,char**v,char**e){
  long P=rs0(20);
  long pid=rs0(2);
  long M=rs0(20);
  long PP=rs0(39);
  o_lbl("who="); o_putn(M==P?0:1); o_lbl(" P=");o_putn(P); o_lbl(" M=");o_putn(M); o_lbl(" PP=");o_putn(PP); o_lbl(" forkrc=");o_putn(pid); o_lbl("\n");
  return 0;
}
