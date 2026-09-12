#include "rs.h"
// Try installing via libc sigaction and see whether a raw handler gets
// called at all. If libc's sigaction works but raw doesn't, the kernel
// needs the libc trampoline (SA_ flags or a restorer).
static volatile long g_hits=0;
static void h2(int s){(void)s; g_hits++; rs1(1,0x5b);}
int main(int argc,char**argv,char**envp){
  o_lbl("(no libc here; raw only) - test sigaction with flags=0x40 SA_SIGINFO\n");
  return 0;
}
