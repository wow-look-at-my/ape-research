#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <unistd.h>
int main(void){ pid_t p0=getpid(); pid_t x=fork(); pid_t p1=getpid();
  printf("p0=%d p1=%d ppid=%d fork_rc=%d is_child=%d\n",p0,p1,getppid(),(int)x,p1!=p0); return 0; }
