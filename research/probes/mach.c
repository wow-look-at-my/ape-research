// mach.c - isolate which syscall classes work on macOS arm64, using raw SVC only.
// Each test runs in its own process so a signal does not stop the sweep.
// Progress is reported with raw write(2) so nothing hides in a stdio buffer.

typedef long i64;
typedef unsigned long u64;

static void w(const char *s, u64 n) {
	register i64 x0 __asm__("x0") = 2;
	register i64 x1 __asm__("x1") = (i64)s;
	register i64 x2 __asm__("x2") = (i64)n;
	register i64 x16 __asm__("x16") = 0x2000004; // BSD write
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
}
static void ws(const char *s) { u64 n = 0; while (s[n]) n++; w(s, n); }
static void wh(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){ int d=(int)((v>>(60-4*i))&0xf); b[2+i]= d<10?(char)('0'+d):(char)('a'+d-10); }
	b[18]='\n'; w(b,19);
}

// Raw syscall: class in the number, args in x0..x3.
static i64 svc(i64 n, i64 a, i64 b, i64 c, i64 d) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x3 __asm__("x3") = d;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0)
	                 : "r"(x1), "r"(x2), "r"(x3), "r"(x16)
	                 : "x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
	return x0;
}
#define BSD(n)  (0x2000000 | (n))
#define MACH(n) (0x1000000 | (n))

static void start(const char *name) { ws("--- "); ws(name); ws("\n"); }
static void result(i64 r) { ws("    returned "); wh((u64)r); }

void go(void) {
	ws("A raw BSD write/getpid\n");
	result(svc(BSD(20), 0, 0, 0, 0));        // getpid

	start("B BSD bogus number 0x123");
	result(svc(BSD(0x123), 0, 0, 0, 0));
	ws("    (did not trap)\n");

	start("C mach trap 3 mach_absolute_time");
	result(svc(MACH(3), 0, 0, 0, 0));

	start("D mach trap 3 again");
	result(svc(MACH(3), 0, 0, 0, 0));

	start("E mach trap 27 thread_self");
	result(svc(MACH(27), 0, 0, 0, 0));

	start("F mach trap 31 mach_msg_trap (null args)");
	result(svc(MACH(31), 0, 0, 0, 0));

	start("G mach trap 28 task_self");
	result(svc(MACH(28), 0, 0, 0, 0));

	start("H mach trap 2 _kernelrpc_mach_vm_allocate_trap");
	result(svc(MACH(2), 0, 0x1000, 0x1000, 1));

	ws("ALL TESTS COMPLETED\n");
}

__asm__(".globl _start\n"
        "_start:\n"
        "\tbl _go\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
