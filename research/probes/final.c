// Zero-import loader shape: LC_MAIN entry, raw SVC syscalls, libSystem declared but unused.
typedef unsigned long u64;
static u64 raw3(u64 n,u64 a,u64 b,u64 c){
  register u64 x0 __asm__("x0")=a; register u64 x1 __asm__("x1")=b;
  register u64 x2 __asm__("x2")=c; register u64 x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):
    "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0;}
static void ws(const char*s){u64 n=0;while(s[n])n++;raw3(0x2000004,2,(u64)s,n);}
int main(int argc, char **argv){ (void)argv; ws("ZERO-IMPORT LC_MAIN OK\n"); ws("argc>1: "); ws(argc>1?"yes\n":"no\n"); return 0; }
