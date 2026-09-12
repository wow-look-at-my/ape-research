#include "rs.h"
int main(int argc,char**argv,char**envp){
  // pselect with zero timeout (must return 0, not block)
  { long ts[2]={0,0}; o_lbl("pselect(0,NULL,ts=0)="); o_putn(rs6(394,0,0,0,0,(long)ts,0)); o_lbl("\n"); }
  // fork/pipe/execve
  { int fds[2]; long r=rs1(42,(long)fds); o_lbl("pipe(42)="); o_putn(r); o_lbl(" r="); o_putn(fds[0]); o_lbl(" w="); o_putn(fds[1]); o_lbl("\n");
    long pid=rs0(2); o_lbl("fork(2)="); o_putn(pid); o_lbl("\n");
    if(pid==0){ rs3(4,fds[1],(long)"CHILD\n",6); rs1(1,0); }
    else { char b[16]; long n=rs3(3,fds[0],(long)b,16); o_lbl("parent read n="); o_putn(n); o_lbl(" "); o_write(1,b,n); 
           int st=0; o_lbl("wait4="); o_putn(rs4(7,pid,(long)&st,0,0)); o_lbl(" status="); o_putn(st); o_lbl("\n"); } }
  // nanosleep
  { long ts[2]={0,1000000}; o_lbl("nanosleep 1ms="); o_putn(rs2(101,(long)ts,0)); o_lbl("\n"); }
  // clock_gettime? (no SYS_clock_gettime on darwin)
  { long ts[2]; o_lbl("clock_gettime(113) as BSD="); o_putn(rs2(113,(long)ts,0)); o_lbl(" (expect negative ENOSYS)\n"); }
  // getentropy with buffer
  { unsigned char buf[16]; for(int i=0;i<16;i++)buf[i]=0;
    long r=rs2(500,(long)buf,16); o_lbl("getentropy r="); o_putn(r); o_lbl(" bytes="); for(int i=0;i<16;i++)o_puth(buf[i]); o_lbl("\n"); }
  // issetugid raw (327)
  { o_lbl("issetugid(327)="); o_putn(rs0(327)); o_lbl("\n"); }
  // thread_selfid (372) -> uint64 tid
  { o_lbl("thread_selfid(372)="); o_putn(rs0(372)); o_lbl("\n"); }
  // sem_open (268)
  { o_lbl("sem_open raw(268)=\n"); }
  // sysctl variants
  { unsigned int ncpu=0; size_t len=sizeof ncpu; int mib[2]={6,3};
    long r=rs6(202,(long)mib,2,(long)&ncpu,(long)&len,0,0);
    o_lbl("sysctl {6,3} r="); o_putn(r); o_lbl(" len="); o_putn((long)len); o_lbl(" val="); o_putn(ncpu); o_lbl("\n"); }
  { char buf[256]; size_t len=sizeof buf; int mib[2]={1,10};
    long r=rs6(202,(long)mib,2,(long)buf,(long)&len,0,0);
    o_lbl("sysctl {1,10} kern.hostname r="); o_putn(r); o_lbl(" len="); o_putn((long)len); o_lbl(" val="); o_putn((long)buf); o_lbl("\n"); }
  // mach traps
  { o_lbl("mach task_self (trap 28)="); o_puth((unsigned long)mt0(28)); o_lbl("\n"); }
  { o_lbl("mach mach_absolute_time (trap ?) via 3? ="); o_puth((unsigned long)mt0(3)); o_lbl("\n"); }
  { o_lbl("mach host_self (trap 29)="); o_puth((unsigned long)mt0(29)); o_lbl("\n"); }
  return 0;
}
