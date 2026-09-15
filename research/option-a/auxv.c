#include <stdint.h>
#include <stddef.h>
typedef uint64_t u64;
extern u64 saved_x3, saved_x0;
static inline long sys3(long n,long a,long b,long c){register long x16 __asm__("x16")=n;register long x0 __asm__("x0")=a,x1 __asm__("x1")=b,x2 __asm__("x2")=c;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x16),"r"(x1),"r"(x2):"memory","cc");return x0;}
static void out(const char*s,long n){sys3(0x2000000|4,1,(long)s,n);}
static void outs(const char*s){long n=0;while(s[n])n++;out(s,n);}
static void outhex(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(v>>((15-i)*4))&0xf;b[2+i]=d<10?'0'+d:'a'+d-10;}b[18]=' ';out(b,19);}
static void outdec(long v){if(v<0){outs("-");v=-v;}char b[24];int i=23;b[i]=' ';if(!v)b[--i]='0';while(v){b[--i]='0'+v%10;v/=10;}out(b+i,24-i);}
int main(void){
  u64 sp; __asm__ volatile("mov %0, sp":"=r"(sp));
  u64 *p=(u64*)sp;
  // argc, argv.., NULL, envp.., NULL, auxv (pairs, terminated by AT_NULL=0)
  long argc=(long)p[0];
  outs("argc="); outdec(argc); outs(" sp="); outhex(sp); outs("\n");
  u64 *q=p+1+argc+1;
  while(*q)q++; q++; // skip envp
  outs("auxv:\n");
  for(int i=0;i<64 && q[0]!=0;i++,q+=2){
    outs("  type="); outdec((long)q[0]); outs(" val="); outhex(q[1]);
    if (q[1]>=0x180000000UL && q[1]<0x1A0000000UL) outs("  <-- cache-window ptr");
    outs("\n");
  }
  sys3(0x2000000|1,0,0,0); return 0;
}
