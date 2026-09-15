// sigsys.c - does macOS ever raise SIGSYS for raw SVC? Test each syscall class.
// A SIGSYS handler records the fact and we continue, so one process can test many.

typedef long i64;
typedef unsigned long u64;

static void w(const char *s, u64 n) {
	register i64 x0 __asm__("x0") = 2;
	register i64 x1 __asm__("x1") = (i64)s;
	register i64 x2 __asm__("x2") = (i64)n;
	register i64 x16 __asm__("x16") = 0x2000004;
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
}
static void ws(const char *s) { u64 n = 0; while (s[n]) n++; w(s, n); }
static void wh(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){ int d=(int)((v>>(60-4*i))&0xf); b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10); }
	b[18]='\n'; w(b,19);
}

// A SIGSYS handler, installed with the BSD sigaction syscall (#46) directly.
// struct sigaction on arm64 XNU: handler, mask(4), flags(4), then more.
typedef void (*sighandler)(int);
struct ksa { sighandler handler; unsigned mask; int flags; };

static i64 svc4(i64 n, i64 a, i64 b, i64 c) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
	return x0;
}

static volatile int sigsys_seen;

static void on_sigsys(int sig) { sigsys_seen = sig; }

static void probe(const char *label, i64 num) {
	sigsys_seen = 0;
	i64 r = svc4(num, 0, 0, 0);
	ws(label); ws(" -> ret "); wh((u64)r);
	if (sigsys_seen) { ws("    SIGNAL "); wh((u64)sigsys_seen); }
}

void go(void) {
	struct ksa sa; sa.handler = on_sigsys; sa.mask = 0; sa.flags = 0;
	// SIGSYS is 12 on XNU. BSD sigaction = 46.
	svc4(0x2000000 | 46, 12, (i64)&sa, 0);
	ws("SIGSYS handler installed (sigaction=46, SIGSYS=12)\n\n");

	probe("class0 #0   (unix/legacy)   ", 0x0000000);
	probe("class0 #1   (legacy exit)   ", 0x0000001);
	probe("class0 #4   (legacy write)  ", 0x0000004);
	probe("class1 #3   (mach trap)     ", 0x1000003);
	probe("class2 #20  (BSD getpid)    ", 0x2000014);
	probe("class2 #1   (BSD exit)      ", 0x2000001);
	probe("class3 #0   (diagnostics)   ", 0x3000000);
	probe("class4 #0   (machdep)       ", 0x4000000);
	probe("class5 #0   (machdep)       ", 0x5000000);
	probe("huge    #0x7fffffff         ", 0x7fffffff);
	probe("BSD deprecated #0 (syscall) ", 0x2000000);

	ws("\nDONE - no process was killed\n");
}

__asm__(".globl _start\n"
        "_start:\n"
        "\tbl _go\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
