#ifndef RS_H
#define RS_H
#include <stdint.h>
#include <stddef.h>
long rs_syscall6(long classnum, long a, long b, long c, long d, long e, long f);
static inline long rs6(long n,long a,long b,long c,long d,long e,long f){return rs_syscall6(0x2000000L|n,a,b,c,d,e,f);}
static inline long rs5(long n,long a,long b,long c,long d,long e){return rs_syscall6(0x2000000L|n,a,b,c,d,e,0);}
static inline long rs4(long n,long a,long b,long c,long d){return rs_syscall6(0x2000000L|n,a,b,c,d,0,0);}
static inline long rs3(long n,long a,long b,long c){return rs_syscall6(0x2000000L|n,a,b,c,0,0,0);}
static inline long rs2(long n,long a,long b){return rs_syscall6(0x2000000L|n,a,b,0,0,0,0);}
static inline long rs1(long n,long a){return rs_syscall6(0x2000000L|n,a,0,0,0,0,0);}
static inline long rs0(long n){return rs_syscall6(0x2000000L|n,0,0,0,0,0,0);}
static inline long mt6(long n,long a,long b,long c,long d,long e,long f){return rs_syscall6(0x1000000L|n,a,b,c,d,e,f);}
static inline long mt0(long n){return rs_syscall6(0x1000000L|n,0,0,0,0,0,0);}
static long o_write(long fd,const void*b,long n){return rs3(4,fd,(long)b,n);}
static void o_puts(const char*s){long l=0;while(s[l])l++;o_write(1,s,l);}
static void o_putn(long v){char b[32];int i=32;int neg=v<0;unsigned long u=neg?(unsigned long)(-v):(unsigned long)v;
 if(!u)b[--i]='0';while(u){b[--i]='0'+(u%10);u/=10;}if(neg)b[--i]='-';o_write(1,b+i,32-i);}
static void o_puth(unsigned long v){char b[32];int i=32;if(!v)b[--i]='0';while(v){int d=v&15;b[--i]=d<10?'0'+d:'a'+d-10;v>>=4;}o_write(1,b+i,32-i);}
static void o_lbl(const char*s){o_puts(s);}
static void o_putsn(const char*s,long n){o_write(1,s,n);}
#endif
