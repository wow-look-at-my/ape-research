#include "rs.h"
static struct { unsigned long handler; unsigned int mask; int flags; } g_sa;
static volatile long g_n=0;
static void handler(int sig, void*si, void*ctx){ (void)si;(void)ctx;
  g_n++;
  // Return: kernel's raw delivery must restore state for a plain ret.
  // If it does, we'll fall back to the faulting instruction => loop. So
  // instead, longjmp-free: set a flag in a register we control is hard.
  // Fastest proof of delivery: write from the handler directly.
  char m[8]; m[0]='H'; m[1]='R'; m[2]='!'; m[3]='\n';
  rs3(4,2,(long)m,4);
}
int main(int argc,char**argv,char**envp){
  g_sa.handler=(unsigned long)handler; g_sa.mask=0; g_sa.flags=0;
  o_lbl("SA_SIGINFO=0x40 test\n");
  o_lbl("install="); o_putn(rs3(46,10,(long)&g_sa,0)); o_lbl("\n");
  __asm__ volatile("msr daifclr, #2");  // ensure interrupts unmasked
  o_lbl("faulting...\n");
  volatile unsigned long v=*(volatile unsigned long*)0x100000000UL;
  o_lbl("returned v="); o_putn((long)v); o_lbl(" hits="); o_putn(g_n); o_lbl("\n");
  rs1(1,0);
}
