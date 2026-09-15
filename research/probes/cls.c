typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
static i64 svc(i64 n){register i64 x0 __asm__("x0")=0;register i64 x16 __asm__("x16")=n;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");return x0;}
/* Run the caller's chosen syscall number, taken from argv-free constant. */
void t0(void){ws("0x0        -> ");wh((u64)svc(0x0));}
void t1(void){ws("0x1        -> ");wh((u64)svc(0x1));}
void t400(void){ws("0x400      -> ");wh((u64)svc(0x400));}
void t1m(void){ws("0x1000000  -> ");wh((u64)svc(0x1000000));}
void t2m(void){ws("0x2000000  -> ");wh((u64)svc(0x2000000));}
void go(void){
  ws("Each line is a raw SVC with this exact x16.\n");
  t1(); t400(); t1m(); t2m(); t0();   /* t0 last: it is the one expected to die */
  ws("ALL DONE\n");
}
__asm__(".globl _start\n_start:\n\tbl _go\n\tmovz x16, #0x200, lsl #16\n\tadd x16, x16, #1\n\tmov x0, #0\n\tsvc #0x80\n");
