#include "rs.h"
volatile unsigned long g_hits=0;
__attribute__((naked)) static void h(void){
  __asm__ volatile(
    // write(2, "HANDLER\n", 8) then exit(0x5e)
    "adrp x0, msg@PAGE\n\t"
    "add  x0, x0, msg@PAGEOFF\n\t"
    "mov  x1, #8\n\t"
    "mov  x2, x0\n\t"
    "mov  x0, #2\n\t"
    "mov  x1, x2\n\t"
    "mov  x2, #8\n\t"
    "mov  x16, #4\n\t"
    "svc  #0x80\n\t"
    "mov  x0, #0x5e\n\t"
    "mov  x16, #1\n\t"
    "svc  #0x80\n\t");
}
struct asa { unsigned long handler; unsigned int mask; int flags; };
int main(int c,char**v,char**e){
  struct asa sa; sa.handler=(unsigned long)h; sa.mask=0; sa.flags=0;
  o_lbl("install="); o_putn(rs3(46,11,(long)&sa,0)); o_lbl("\n");
  o_lbl("faulting\n");
  volatile unsigned long x=*(volatile unsigned long*)0; (void)x;
  o_lbl("no fault?\n"); rs1(1,0);
}
__asm__(".data\nmsg: .ascii \"HANDLER\\n\"\n");
