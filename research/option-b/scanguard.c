#include "rs.h"
// scanguard LO HI : for each BSD syscall N in [LO,HI], fork; child calls
// rs0(N) and prints; parent waits with a spin guard, kills on hang.
static void putnum_buf(char*b,int*n,long v){ (void)b;(void)n;(void)v; }
int main(int argc,char**argv,char**envp){
  long lo=0,hi=0;
  { const char*s=argv[1]; while(*s)lo=lo*10+(*s++-'0'); s=argv[2]; while(*s)hi=hi*10+(*s++-'0'); }
  for(long n=lo;n<=hi;n++){
    long pid=rs0(2);
    if(pid==0){
      long r=rs0(n);
      o_lbl("RET rc="); o_putn(r); o_lbl("\n");
      rs1(1,0);
    }
    int st=0; long got=0;
    for(long spin=0; spin<3000000; spin++){
      long w=rs4(7,pid,(long)&st,1,0); // WNOHANG
      if(w==pid){got=1;break;}
      if(w<0){got=-1;break;}
      // tiny pause
      for(volatile int k=0;k<200;k++){}
    }
    if(!got){ rs2(37,pid,9); o_lbl("SC "); o_putn(n); o_lbl(" HANG\n"); continue; }
    if(got<0){ o_lbl("SC "); o_putn(n); o_lbl(" WAITERR\n"); continue; }
    // decode
    int low7 = st & 0x7f;
    if(low7==0){ /* exited */ }
    else if(low7!=0x7f){ o_lbl("SC "); o_putn(n); o_lbl(" SIGNAL "); o_putn(low7); o_lbl("\n"); }
  }
  return 0;
}
