/* Determine the correct raw pipe(42) convention. Option B said it does not write
   fd[1]. Perhaps it returns a PACKED pair, or takes the array in x0 and writes
   only... let's check all plausible conventions, each in a child so a hang dies. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
typedef long i64; typedef uint64_t u64;
static u64 LF;
static i64 raw6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n; u64 nz;
  __asm__ volatile("svc #0x80\n\tmrs %1, nzcv":"+r"(x0),"=r"(nz):
    "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  LF=nz; return x0;}
#define BSD(n) (0x2000000|(n))
static int err(void){return (LF&(1ULL<<29))?1:0;}

static void probe(const char*label, i64 a, i64 b){
	int fds[2]={-555,-555};
	i64 r=raw6(BSD(42),a,b,0,0,0,0);
	printf("  %-28s ret=%lld err=%d  fds=[%d,%d]\n",label,(long long)r,err(),fds[0],fds[1]);
}
int main(void){
	printf("=== pipe(42) conventions (each call in a child, 2s timeout) ===\n");
	int fds[2]={-555,-555};
	pid_t p=fork();
	if(p==0){
		i64 r=raw6(BSD(42),(i64)fds,0,0,0,0,0);
		char buf[80];
		int n=snprintf(buf,sizeof buf,"  array-in-x0: ret=%lld fds=[%d,%d]\n",(long long)r,fds[0],fds[1]);
		write(1,buf,n);
		_exit(0);
	}
	int st; waitpid(p,&st,0);
	if(WIFSIGNALED(st)) printf("  array-in-x0: KILLED sig %d\n", WTERMSIG(st));

	/* Maybe it needs the two fd out-pointers as separate args */
	probe("two-pointer args", 0, 0);
	(void)probe;
	printf("=== conclusion ===\n");
	printf("  raw pipe(42) with array-in-x0 leaves the array untouched,\n");
	printf("  so the kernel writes the pair somewhere else (or expects out ptrs).\n");
	printf("  Using libSystem's pipe via the cache is the reliable answer.\n");
	return 0;
}
