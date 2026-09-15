#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdlib.h>
// fork a child to read a byte/word at addr; report success/fault
static int try_read(uint64_t a, unsigned char*out){
  pid_t p=fork();
  if(p==0){ volatile unsigned char v=*(volatile unsigned char*)a; _exit(v); }
  int st=0; waitpid(p,&st,0);
  if(WIFEXITED(st)){ *out=(unsigned char)WEXITSTATUS(st); return 0; }
  return -1;
}
int main(int argc,char**argv){
  uint64_t addrs[]={0x100000000ULL,0x180000000ULL,0x188830000ULL,0x188cdd000ULL,0x198b70000ULL,0xdead0000ULL,0x1f4bec1a0ULL,0x188963e00ULL,0x1ff05c000ULL,0};
  for(int i=0;addrs[i];i++){
    unsigned char v=0; int r=try_read(addrs[i],&v);
    char vec[2]; vec[0]=0;
    long mc=syscall(SYS_mincore,(void*)(addrs[i]&~0x3fffULL),(size_t)16384,vec);
    printf("addr=%#llx read=%s byte=%02x  mincore=%ld vec0=%02x\n",
      (unsigned long long)addrs[i], r?"FAULT":"ok", v, mc, (unsigned char)vec[0]);
    fflush(stdout);
  }
  return 0;
}
