typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
static i64 svc(i64 n,i64 a){register i64 x0 __asm__("x0")=a;register i64 x16 __asm__("x16")=n;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");return x0;}
/* class 0 = legacy table. #0 is "indirect syscall" (takes the number from x0). */
void a(void){ws("class0 #2 (fork)        -> ");wh((u64)svc(0x2,0));}
void b(void){ws("class0 #4 (write)       -> ");wh((u64)svc(0x4,0));}
void c(void){ws("class0 #0x60 (invalid)  -> ");wh((u64)svc(0x60,0));}
void d(void){ws("class0 #0 indirect,x0=0 -> ");wh((u64)svc(0x0,0));}
void go(void){ws("class 0 legacy syscall table\n");a();b();c();d();ws("ALL DONE\n");}
__asm__(".globl _start\n_start:\n\tbl _go\n\tmovz x16, #0x200, lsl #16\n\tadd x16, x16, #1\n\tmov x0, #0\n\tsvc #0x80\n");
