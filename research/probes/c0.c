typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static i64 svc(i64 n,i64 a){register i64 x0 __asm__("x0")=a;register i64 x16 __asm__("x16")=n;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");return x0;}
/* Each class in its own function so we can see which one never returns. */
void a_class0(void){ ws("class0 #0 (syscall) ...\n"); svc(0x0,0); ws("  returned\n"); }
void b_class1(void){ ws("class1 #3 (mach) ...\n"); svc(0x1000003,0); ws("  returned\n"); }
void c_class2(void){ ws("class2 #20 (BSD getpid) ...\n"); svc(0x2000014,0); ws("  returned\n"); }
void d_class3(void){ ws("class3 #0 ...\n"); svc(0x3000000,0); ws("  returned\n"); }
void e_class4(void){ ws("class4 #0 ...\n"); svc(0x4000000,0); ws("  returned\n"); }
void go(void){ ws("start\n"); a_class0(); b_class1(); c_class2(); d_class3(); e_class4(); ws("ALL DONE\n"); }
__asm__(".globl _start\n_start:\n\tbl _go\n\tmovz x16, #0x200, lsl #16\n\tadd x16, x16, #1\n\tmov x0, #0\n\tsvc #0x80\n");
