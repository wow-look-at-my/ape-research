#include "rs.h"
int main(int argc,char**argv,char**envp){
  // --- mmap/mprotect/munmap ---
  long p = rs6(197,0,65536,3,0x1002,-1,0);          // MAP_ANON|MAP_PRIVATE, RW
  o_lbl("mmap anon="); o_putn(p); o_lbl("\n");
  if(p>0){
    *(volatile char*)p=42;
    o_putn(rs3(74,p,65536,1)); o_lbl(" = mprotect R\n");   // PROT_READ
    o_putn(rs3(74,p,65536,3)); o_lbl(" = mprotect RW\n");
    *(volatile char*)p=43;
    o_putn(rs2(73,p,65536)); o_lbl(" = munmap\n");
  }
  // --- pread/pwrite on a file we open with open(5) ---
  long fd=rs3(5,(long)"/tmp/apex-b/rs.h",0,0);
  o_lbl("open fd="); o_putn(fd); o_lbl("\n");
  if(fd>=0){ char buf[16]; long r=rs3(153,fd,(long)buf,16); o_lbl("pread="); o_putn(r); o_lbl("\n"); }
  // --- getrlimit / setrlimit ---
  { long rl[2]; long r=rs2(194,3,(long)rl); o_lbl("getrlimit(RLIMIT_STACK)="); o_putn(r); o_lbl(" cur="); o_putn(rl[0]); o_lbl("\n");
    r=rs2(195,3,(long)rl); o_lbl("setrlimit same="); o_putn(r); o_lbl("\n"); }
  // --- pselect with all-null ---
  o_lbl("pselect(null)="); o_putn(rs6(394,0,0,0,0,0,0)); o_lbl("\n");
  // --- sigaltstack ---
  { long ss[3]={0,0,0}; o_lbl("sigaltstack(NULL,&old)="); o_putn(rs2(53,0,(long)ss)); o_lbl(" sp="); o_putn(ss[0]); o_lbl(" size="); o_putn(ss[1]); o_lbl(" flags="); o_putn(ss[2]); o_lbl("\n"); }
  // --- sigaction (query SIGUSR1) ---
  { long osa[3]={0,0,0}; o_lbl("sigaction(SIGUSR1=30,NULL,&old)="); o_putn(rs3(46,30,0,(long)osa)); o_lbl(" handler="); o_putn(osa[0]); o_lbl(" mask="); o_putn(osa[1]); o_lbl(" flags="); o_putn(osa[2]); o_lbl("\n"); }
  // --- sigprocmask (query) ---
  { int om=0; o_lbl("sigprocmask(SIG_BLOCK=1,NULL,&old)="); o_putn(rs3(48,1,0,(long)&om)); o_lbl(" mask="); o_putn(om); o_lbl("\n"); }
  // --- gettimeofday ---
  { long tv[2]; o_lbl("gettimeofday="); o_putn(rs2(116,(long)tv,0)); o_lbl(" sec="); o_putn(tv[0]); o_lbl(" usec="); o_putn(tv[1]); o_lbl("\n"); }
  // --- invalid class-1 syscall should return ENOSYS, not trap ---
  o_lbl("bogus class1 #9999="); o_putn(rs0(9999)); o_lbl(" (expect -78 ENOSYS)\n");
  return 0;
}
