#include "rs.h"
int main(int argc,char**argv,char**envp){
  o_lbl("== basic identity/raw syscalls ==\n");
  o_lbl("getpid(20)="); o_putn(rs0(20)); o_lbl("\n");
  o_lbl("getuid(24)="); o_putn(rs0(24)); o_lbl("\n");
  o_lbl("getppid(39)="); o_putn(rs0(39)); o_lbl("\n");
  o_lbl("geteuid(25)="); o_putn(rs0(25)); o_lbl("\n");
  o_lbl("getgid(47)="); o_putn(rs0(47)); o_lbl("\n");
  o_lbl("getegid(43)="); o_putn(rs0(43)); o_lbl("\n");
  o_lbl("issetugid(147)="); o_putn(rs0(147)); o_lbl("\n");
  o_lbl("getentropy(500)="); o_putn(rs2(500,(long)argv,16)); o_lbl("\n");
  o_lbl("sysctl(202, {1,3})"); {
    int mib[2]={1,3}; unsigned int ncpu=0; size_t len=sizeof ncpu;
    long r=rs6(202,(long)mib,2,(long)&ncpu,(long)&len,0,0);
    o_lbl(" r="); o_putn(r); o_lbl(" ncpu="); o_putn(ncpu); o_lbl("\n");
  }
  o_lbl("sysctl(202, {6,3}) kern.hostname"); {
    int mib[2]={6,3}; char buf[256]; size_t len=sizeof buf;
    long r=rs6(202,(long)mib,2,(long)buf,(long)&len,0,0);
    o_lbl(" r="); o_putn(r); o_lbl(" ="); o_puts(buf); o_lbl("\n");
  }
  return 0;
}
