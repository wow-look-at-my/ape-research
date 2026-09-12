#include "rs.h"
volatile unsigned long g_hits=0, g_arg=0;
__attribute__((naked)) static void h(void){
  __asm__ volatile(
    "adrp x9, _g_hits@PAGE\n\t"
    "add  x9, x9, _g_hits@PAGEOFF\n\t"
    "mov  x10, #12345\n\t"
    "str  x10, [x9]\n\t"
    "mov  x0, #0x5c\n\t"
    "mov  x16, #1\n\t"
    "svc  #0x80\n\t");
}
struct asa { unsigned long handler; unsigned int mask; int flags; };
int main(int c,char**v,char**e){
  struct asa sa; sa.handler=(unsigned long)h; sa.mask=0; sa.flags=0x40;
  o_lbl("install SIGSEGV="); o_putn(rs3(46,11,(long)&sa,0)); o_lbl("\n");
  o_lbl("reading *0 ...\n");
  volatile unsigned long x=*(volatile unsigned long*)0;
  o_lbl("survived x="); o_putn((long)x); o_lbl("\n"); rs1(1,0);
}
