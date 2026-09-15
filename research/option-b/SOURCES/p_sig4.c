#include "rs.h"
static unsigned long g_h=0; static volatile long g_hits=0;
// A naked handler: do the minimum, then exit via raw syscall.
__attribute__((naked)) static void handler1(int sig){
  __asm__ volatile(
    "adrp x9, _g_hits@PAGE\n\t"
    "add  x9, x9, _g_hits@PAGEOFF\n\t"
    "ldr  x10, [x9]\n\t"
    "add  x10, x10, #1\n\t"
    "str  x10, [x9]\n\t"
    "mov  x0, #0x5a\n\t"
    "mov  x16, #1\n\t"
    "svc  #0x80\n\t"
    "brk  #1\n\t");
}
static struct { unsigned long handler; unsigned int mask; int flags; } g_sa;
int main(int argc,char**argv,char**envp){
  g_sa.handler=(unsigned long)handler1; g_sa.mask=0; g_sa.flags=0;
  o_lbl("SIGBUS install rc="); o_putn(rs3(46,10,(long)&g_sa,0)); o_lbl("\n");
  char m[]="faulting\n"; rs3(4,2,(long)m,8);
  volatile unsigned long v=*(volatile unsigned long*)0x100000000UL;
  o_lbl("returned v="); o_putn((long)v); o_lbl(" hits="); o_putn(g_hits); o_lbl("\n");
  rs1(1,0);
}
