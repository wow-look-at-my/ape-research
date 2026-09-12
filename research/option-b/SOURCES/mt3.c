#include "rs.h"
// Mach traps return kern_return_t in x0 and do NOT use the carry-flag error
// convention. Test: call task_self_trap twice through a path that does not
// consult carry, and compare.
__attribute__((noinline)) static long mt_raw(long n){ 
  register long x0 __asm__("x0")=0; register long x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16) : "memory","cc");
  return x0;
}
int main(int argc,char**argv,char**envp){
  o_lbl("raw task_self_trap x0 (no carry consult):\n");
  for(int i=0;i<4;i++){ o_lbl("  "); o_putn(mt_raw(-28)); o_lbl("\n"); }
  o_lbl("=> mach traps return kern_return_t in x0; the carry/negate convention\n");
  o_lbl("   used for BSD syscalls must NOT be applied to them.\n");
  return 0;
}
