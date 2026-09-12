#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
int main(void){
  long orig=getpid();
  long pid=syscall(SYS_fork);
  long me=getpid();
  if(pid!=0 && me!=orig){ printf("CHILDvia syscall(SYS_fork): rc=%ld me=%ld\n",pid,me); _exit(0); }
  if(pid==0){ printf("CHILDvia rc==0: me=%ld\n",me); _exit(0); }
  printf("PARENT rc=%ld me=%ld\n",pid,me);
  wait4(pid,0,0,0);
  return 0;
}
