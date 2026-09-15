#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
static void h(int s){ printf("libc handler hit sig=%d\n", s); _exit(0x5b); }
int main(void){
  struct sigaction sa; sa.sa_handler=h; sa.sa_flags=0; sigemptyset(&sa.sa_mask);
  printf("libc sigaction rc=%d\n", sigaction(SIGBUS,&sa,0));
  printf("faulting\n"); fflush(stdout);
  volatile unsigned long v=*(volatile unsigned long*)0x100000000UL;
  printf("returned %lu\n", v);
  return 0;
}
