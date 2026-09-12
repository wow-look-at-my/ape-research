#include "rs.h"
volatile unsigned long g_hits=0;
__attribute__((naked)) static void h(void){
  __asm__ volatile(
    "adrp x9, _g_hits@PAGE\n\t"
    "add  x9, x9, _g_hits@PAGEOFF\n\t"
    "mov  x10, #1\n\t"
    "str  x10, [x9]\n\t"
    "mov  x0, #0x5d\n\t"
    "mov  x16, #1\n\t"
    "svc  #0x80\n\t");
}
struct asa { unsigned long handler; unsigned int mask; int flags; };
int main(int c,char**v,char**e){
  struct asa sa; sa.handler=(unsigned long)h; sa.mask=0; sa.flags=0;
  o_lbl("install SIGUSR1(30)="); o_putn(rs3(46,30,(long)&sa,0)); o_lbl("\n");
  o_lbl("raise(SIGUSR1=30) via kill(getpid,30)...\n");
  long pid=rs0(20);
  o_lbl("kill rc="); o_putn(rs2(37,pid,30)); o_lbl("\n");
  for(volatile int i=0;i<1000000;i++){}
  o_lbl("after: hits="); o_putn((long)g_hits); o_lbl("\n");
  rs1(1,0);
}
