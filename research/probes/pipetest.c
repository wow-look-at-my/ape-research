/* Verify the corrected pipe logic actually produces two usable fds. */
#include <stdio.h>
#include <stdint.h>
typedef long i64; typedef uint64_t u64;
static u64 lastflags;
static i64 raw6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n; u64 nz;
  __asm__ volatile("svc #0x80\n\tmrs %1, nzcv":"+r"(x0),"=r"(nz):
    "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  lastflags=nz; return x0;}
#define BSD(n) (0x2000000|(n))
static i64 s_pipe(int p[2]){
	i64 r=raw6(BSD(42),0,0,0,0,0,0); if(r<0)return r; p[0]=(int)r;
	r=raw6(BSD(42),0,0,0,0,0,0); if(r<0){raw6(BSD(6),p[0],0,0,0,0,0);return r;} p[1]=(int)r;
	return 0;}
int main(void){
	int p[2]={-1,-1};
	i64 r=s_pipe(p);
	printf("s_pipe -> %lld  fds=[%d,%d]\n",(long long)r,p[0],p[1]);
	if(r!=0||p[0]<0||p[1]<0){printf("FAILED\n");return 1;}
	i64 w=raw6(BSD(4),p[1],(i64)"hi",2,0,0,0);
	char buf[8]={0};
	i64 rd=raw6(BSD(3),p[0],(i64)buf,7,0,0,0);
	printf("write=%lld read=%lld got=\"%s\"  %s\n",(long long)w,(long long)rd,buf,
	 (rd==2&&buf[0]=='h')?"PIPE WORKS":"BROKEN");
	raw6(BSD(6),p[0],0,0,0,0,0); raw6(BSD(6),p[1],0,0,0,0,0);
	return 0;}
