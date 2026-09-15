typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
/* Capture result AND the carry flag (NZCV) after svc. */
static i64 svc_flags(i64 n,i64 a,i64 b,i64 c,u64 *flags){
  register i64 x0 __asm__("x0")=a; register i64 x1 __asm__("x1")=b; register i64 x2 __asm__("x2")=c;
  register i64 x16 __asm__("x16")=n; u64 nzcv;
  __asm__ volatile("svc #0x80\n\tmrs %0, nzcv":"+r"(x0),"=r"(nzcv):"r"(x1),"r"(x2),"r"(x16):
    "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  *flags=nzcv; return x0;}
int main(void){
  u64 f;
  i64 r = svc_flags(0x2000004, 1, (i64)"ok\n", 3, &f);   /* write, success */
  ws("write ok:    ret="); wh((u64)r); ws("   nzcv="); wh(f); ws("  C="); ws((f>>29)&1?"1\n":"0\n");
  r = svc_flags(0x2000004, 999, (i64)"ok\n", 3, &f);     /* write to bad fd */
  ws("write EBADF: ret="); wh((u64)r); ws("   nzcv="); wh(f); ws("  C="); ws((f>>29)&1?"1\n":"0\n");
  r = svc_flags(0x2000014, 0,0,0, &f);                   /* getpid, success */
  ws("getpid:      ret="); wh((u64)r); ws("   nzcv="); wh(f); ws("  C="); ws((f>>29)&1?"1\n":"0\n");
  return 0;}
