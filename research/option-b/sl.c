#include <signal.h>
#include <unistd.h>
#include <string.h>
static void h(int s){ const char m[]="HANDLER RAN\n"; write(2,m,sizeof m-1); _exit(0x5b); }
int main(void){
  struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_handler=h;
  const char a[]="install...\n"; write(2,a,sizeof a-1);
  int r=sigaction(SIGSEGV,&sa,0);
  char b[32]; int n=0; b[n++]='r';b[n++]='=';b[n++]='0'+r;b[n++]='\n'; write(2,b,n);
  const char c[]="faulting *0\n"; write(2,c,sizeof c-1);
  volatile unsigned long x=*(volatile unsigned long*)0;
  (void)x;
  return 0;
}
