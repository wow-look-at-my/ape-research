#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
int g=111;
int main(void){
  pid_t pid=fork();
  printf("proc pid=%d ppid=%d forkrc=%d g=%d\n", getpid(), getppid(), (int)pid, g);
  g=222;
  if(pid>0) wait(0);
  return 0;
}
