#include "rs.h"
static struct { unsigned long handler; unsigned int mask; int flags; } g_sa;
static volatile long g_n=0;
// handler installed with flags=0 (SA_SIGINFO clear) => void(int)
static void handler1(int sig){
  g_n=sig;
  rs1(1,sig+100); // exit with a distinctive code: PROVES delivery
}
int main(int argc,char**argv,char**envp){
  g_sa.handler=(unsigned long)handler1; g_sa.mask=0; g_sa.flags=0;
  o_lbl("install SIGBUS(10) flags=0 rc="); o_putn(rs3(46,10,(long)&g_sa,0)); o_lbl("\n");
  o_lbl("faulting...\n");
  volatile unsigned long v=*(volatile unsigned long*)0x100000000UL;
  o_lbl("returned v="); o_putn((long)v); o_lbl(" hits="); o_putn(g_n); o_lbl("\n");
  rs1(1,0);
}
