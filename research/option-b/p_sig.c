#include "rs.h"
static volatile long g_hits=0;
static volatile int g_sig=0;
static volatile unsigned long g_fault=0;
// Apple struct sigaction: {handler, mask u32, flags i32} = 16 bytes
struct asa { unsigned long handler; unsigned int mask; int flags; };
static void handler(int sig){
  // can't use much here; just record and skip. We must not return normally
  // unless the kernel gave us a proper trampoline. Test both.
  g_hits++; g_sig=sig;
  rs1(1,0); // exit immediately: if we get here, delivery worked
}
__attribute__((noreturn)) static void try(void){
  struct asa sa; sa.handler=(unsigned long)handler; sa.mask=0; sa.flags=0;
  long r=rs3(46,10,(long)&sa,0);  // SIGBUS=10
  o_lbl("sigaction(SIGBUS)="); o_putn(r); o_lbl("\n");
  struct asa sb; sb.handler=(unsigned long)handler; sb.mask=0; sb.flags=0;
  r=rs3(46,11,(long)&sb,0);       // SIGSEGV=11
  o_lbl("sigaction(SIGSEGV)="); o_putn(r); o_lbl("\n");
  o_lbl("about to fault at 0x100000000...\n");
  g_fault=*(volatile unsigned long*)0x100000000UL;
  o_lbl("survived?! g_fault="); o_putn((long)g_fault); o_lbl("\n");
  rs1(1,0);
}
int main(int argc,char**argv,char**envp){ try(); }
