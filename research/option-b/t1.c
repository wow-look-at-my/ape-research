#include <stdint.h>
static long raw(long n, long a, long b, long c, long d, long e, long f, long g, long h) {
  register long x0 __asm__("x0")=a; register long x1 __asm__("x1")=b;
  register long x2 __asm__("x2")=c; register long x3 __asm__("x3")=d;
  register long x4 __asm__("x4")=e; register long x5 __asm__("x5")=f;
  register long x8 __asm__("x8")=n;
  register long x16 __asm__("x16")=0x2000000|n;
  __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x8),"r"(x16) : "memory","cc");
  return x0;
}
static long w(long n, long a, long b, long c, long d, long e, long f, long g, long h){ return raw(n,a,b,c,d,e,f,g,h); }
static void puts_(const char*s){ long l=0; while(s[l])l++; w(4,1,(long)s,l,0,0,0,0); }
static void putn(long v){ char b[24]; int i=24; if(v<0){puts_("-");v=-v;} if(!v)b[--i]='0'; while(v){b[--i]='0'+v%10;v/=10;} w(4,1,(long)(b+i),24-i,0,0,0,0); }
int main(int argc, char**argv, char**envp){
  puts_("argc="); putn(argc); puts_(" argv0="); puts_(argv[0]); puts_("\n");
  puts_("write rc="); putn(w(4,1,(long)"hello\n",6,0,0,0,0)); puts_("\n");
  puts_("getpid="); putn(w(20,0,0,0,0,0,0,0)); puts_("\n");
  puts_("mmap="); putn(w(197,0,4096,3,0x1002,-1,0,0)); puts_("\n");
  return 7;
}
