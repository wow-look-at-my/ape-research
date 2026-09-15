#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
int main(void){
  pid_t p0=getpid();
  long x0=syscall(SYS_fork);
  pid_t p1=getpid();
  printf("p0=%d p1=%d ppid=%d fork_x0=%ld is_child=%d\n",p0,p1,getppid(),x0,p1!=p0);
  return 0;
}
