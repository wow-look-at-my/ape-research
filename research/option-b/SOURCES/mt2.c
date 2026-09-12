#include "rs.h"
int main(int argc,char**argv,char**envp){
  // From libSystem stub disassembly: MOVN x16,#imm => x16 = ~imm
  //   task_self_trap:   MOVN #27 -> -28
  //   host_self_trap:   MOVN #28 -> -29
  //   thread_self_trap: MOVN #26 -> -27
  //   mach_reply_port:  MOVN #25 -> -26
  //   mach_msg_trap:    MOVN #30 -> -31
  //   semaphore_signal: MOVN #32 -> -33
  long a=rs_syscall6(-28,0,0,0,0,0,0); o_lbl("x16=-28 task_self_trap   = "); o_putn(a); o_lbl("\n");
  long b=rs_syscall6(-28,0,0,0,0,0,0); o_lbl("x16=-28 again           = "); o_putn(b); o_lbl("  (stable small port name?)\n");
  long c=rs_syscall6(-29,0,0,0,0,0,0); o_lbl("x16=-29 host_self_trap   = "); o_putn(c); o_lbl("\n");
  long d=rs_syscall6(-27,0,0,0,0,0,0); o_lbl("x16=-27 thread_self_trap = "); o_putn(d); o_lbl("\n");
  long e=rs_syscall6(-26,0,0,0,0,0,0); o_lbl("x16=-26 mach_reply_port  = "); o_putn(e); o_lbl("\n");
  o_lbl("all returned normally => negative x16 IS the mach trap encoding\n");
  return 0;
}
