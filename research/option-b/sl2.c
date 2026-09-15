#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
static void h(int s){ const char m[]="RAW-INSTALLED HANDLER RAN\n"; write(2,m,sizeof m-1); _exit(0x5b); }
int main(void){
  struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_handler=h; sa.sa_flags=0;
  long r=syscall(SYS_sigaction, SIGSEGV, &sa, 0);
  printf("raw syscall(SYS_sigaction) rc=%ld sizeof(struct sigaction)=%zu\n", r, sizeof sa);
  printf("faulting\n"); fflush(stdout);
  volatile unsigned long x=*(volatile unsigned long*)0; (void)x;
  return 0;
}
