#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdint.h>
static void h(int s){(void)s;}
int main(void){
  struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_handler=h;
  sigaction(SIGSEGV,&sa,0);
  struct { uintptr_t handler; unsigned int mask; int flags; uintptr_t tramp; } k;
  memset(&k,0,sizeof k);
  long r=syscall(SYS_sigaction, SIGSEGV, 0, &k);
  printf("query rc=%ld handler=%p mask=%#x flags=%#x tramp=%p\n", r,(void*)k.handler,k.mask,k.flags,(void*)k.tramp);
  // Also compare sizeof(struct sigaction)
  printf("sizeof(struct sigaction)=%zu\n", sizeof(struct sigaction));
  return 0;
}
