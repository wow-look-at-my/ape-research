#include "rs.h"
int main(int argc,char**argv,char**envp){
  // task_self_trap: movn x16,#0x37 ; svc  => x16 = -0x38 = -56
  o_lbl("x16=-55 (task_self_trap?): "); o_puth((unsigned long)rs_syscall6(-55,0,0,0,0,0,0)); o_lbl("\n");
  // mach_reply_port: movn x16,#0x32 => -51
  o_lbl("x16=-51 (mach_reply_port?): "); o_puth((unsigned long)rs_syscall6(-51,0,0,0,0,0,0)); o_lbl("\n");
  // thread_self_trap: movn x16,#0x34 => -53
  o_lbl("x16=-53 (thread_self_trap?): "); o_puth((unsigned long)rs_syscall6(-53,0,0,0,0,0,0)); o_lbl("\n");
  return 0;
}
