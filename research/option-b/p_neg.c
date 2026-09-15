#include "rs.h"
static long parsed(const char*s){ int neg=0; if(*s=='-'){neg=1;s++;} long v=0; while(*s)v=v*10+(*s++-'0'); return neg?-v:v; }
int main(int argc,char**argv,char**envp){
  long v=parsed(argv[1]);
  o_lbl("x16="); o_putn(v); o_lbl(" rc="); o_putn(rs_syscall6(v,0,0,0,0,0,0)); o_lbl("\n");
  return 0;
}
