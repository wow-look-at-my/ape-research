#include "rs.h"
static volatile long hits=0;
static void h(int s){(void)s; hits++; rs1(1,0x5c);}
struct asa { unsigned long handler; unsigned int mask; int flags; };
int main(int c,char**v,char**e){
  struct asa sa; sa.handler=(unsigned long)h; sa.mask=0; sa.flags=0x40;
  o_lbl("install SIGSEGV="); o_putn(rs3(46,11,(long)&sa,0)); o_lbl("\n");
  volatile unsigned long*p=0;
  o_lbl("reading *0 ...\n");
  volatile unsigned long x=*p;
  o_lbl("survived x="); o_putn((long)x); o_lbl("\n"); rs1(1,0);
}
