/* Verify Option B's corrections against my own findings. These matter: my loader
   wraps fork/pipe with raw syscalls, so if they are right, my loader is broken. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
typedef long i64; typedef uint64_t u64;
static u64 lastflags;
static i64 raw(i64 n,i64 a,i64 b,i64 c,i64 d){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;
  register i64 x2 __asm__("x2")=c;register i64 x3 __asm__("x3")=d;
  register i64 x16 __asm__("x16")=n; u64 nz;
  __asm__ volatile("svc #0x80\n\tmrs %1, nzcv":"+r"(x0),"=r"(nz):
    "r"(x1),"r"(x2),"r"(x3),"r"(x16):
    "x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  lastflags=nz; return x0;}
static int err(void){ return (lastflags & (1ULL<<29)) ? 1 : 0; }
int main(void){
	/* 1. raw pipe: does it write BOTH fds? Pre-fill with sentinels. */
	int fds[2] = {-777, -777};
	i64 r = raw(0x2000000|42, (i64)fds, 0, 0, 0);
	printf("1. raw pipe(42): ret=%lld err=%d  fds=[%d, %d]  %s\n",
	 (long long)r, err(), fds[0], fds[1],
	 (fds[0]>=0 && fds[1]>=0) ? "BOTH WRITTEN" : "*** fd[1] NOT WRITTEN (Option B correct)");
	if (fds[0]>=0) raw(0x2000000|6, fds[0],0,0,0);
	if (fds[1]>=0) raw(0x2000000|6, fds[1],0,0,0);

	/* 2. is nanosleep(101) really SIGSYS? test in a child so we survive */
	pid_t pid = fork();
	if (pid == 0) {
		struct { long sec, nsec; } ts = {0, 1000000};
		raw(0x2000000|101, (i64)&ts, 0, 0, 0);
		_exit(0);   /* if we get here, no SIGSYS */
	}
	int st=0; waitpid(pid,&st,0);
	printf("2. nanosleep(101) in a child: %s\n",
	 WIFSIGNALED(st) ? "KILLED BY SIGNAL (Option B correct)" : "returned normally");

	/* 3. my earlier claim: mach traps are 0x1000000|n. Option B says negative. */
	i64 t1 = raw(0x1000000|28, 0,0,0,0);
	i64 t2 = raw(-28, 0,0,0,0);
	printf("3. task_self: 0x1000000|28 -> %lld    -28 -> %lld\n", (long long)t1, (long long)t2);
	printf("   (mach_task_self() should be a small port number; if -28 differs, that is the real trap)\n");
	printf("   my 0x1000000|28 value %lld vs BSD syscall 28 = getppid? -> %s\n",
	 (long long)t1, (t1>0 && t1<100000) ? "SMALL: ambiguous" : "not a port");
	return 0;
}
