#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdint.h>
static void h(int s){ (void)s; const char m[]="HANDLER-RAN\n"; write(2,m,sizeof m-1); _exit(0x5b); }
int main(void){
  // kernel struct: {handler, mask(int), flags(int), tramp}
  struct { uintptr_t handler; unsigned int mask; int flags; uintptr_t tramp; } k;
  memset(&k,0,sizeof k); k.handler=(uintptr_t)h; k.flags=0; k.tramp=0;
  long r=syscall(SYS_sigaction, SIGSEGV, &k, 0);
  printf("24-byte struct rc=%ld\n", r); fflush(stdout);
  volatile unsigned long x=*(volatile unsigned long*)0; (void)x;
  return 0;
}
