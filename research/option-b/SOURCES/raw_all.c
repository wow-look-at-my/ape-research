#include "rs.h"
// Consolidated: which Syslib entries have a working raw-syscall replacement?
// Each test is independent; a SIGSYS kills the process so each runs alone.
int main(int argc,char**argv,char**envp){
  int t=0; if(argc>1){ const char*s=argv[1]; while(*s)t=t*10+(*s++-'0'); }
  long r=0;
  switch(t){
    case 0: { long tv[2]={0,0}; r=rs6(93,0,0,0,0,(long)tv,0); break; } // select(93) as nanosleep
    case 1: { char b[16]; r=rs2(500,(long)b,16); break; }             // getentropy(500)
    case 2: { r=rs1(42,(long)(int[2]){0,0}); break; }                 // pipe(42)
    case 3: { r=rs1(2,0); break; }                                    // fork(2)
    case 4: { r=rs0(327); break; }                                    // issetugid(327)
    case 5: { r=rs0(372); break; }                                    // thread_selfid(372)
    case 6: { long ss[3]={0,0,0}; r=rs2(53,0,(long)ss); break; }      // sigaltstack(53)
    case 7: { int mib[2]={6,3}; char buf[64]; size_t l=sizeof buf; r=rs6(202,(long)mib,2,(long)buf,(long)&l,0,0); break; } // sysctl
    case 8: { struct {long c,m;} rl; r=rs2(194,3,(long)&rl); break; } // getrlimit(194)
    case 9: { long ts[2]={0,1000000}; r=rs2(101,(long)ts,0); break; } // nanosleep(101) -> SIGSYS
    case 10:{ long ts[2]={0,0}; r=rs5(93,0,0,0,0,(long)ts); break; }  // select zero
    case 11:{ char b[8]; r=rs3(153,0,(long)b,8); break; }             // pread(153) fd0
    case 12:{ struct {unsigned long h;unsigned m;int f;} sa={0,0,0}; r=rs3(46,10,(long)&sa,0); break; } // sigaction query
    case 13:{ r=mt0(28); break; }                                     // mach task_self
    case 14:{ long ss[3]; r=rs3(53,0,(long)ss,0); break; }            // sigaltstack query
    case 15:{ r=rs0(20); break; }                                     // getpid
  }
  o_lbl("test="); o_putn(t); o_lbl(" rc="); o_putn(r); o_lbl("\n");
  return 0;
}
