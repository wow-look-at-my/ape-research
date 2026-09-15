// sr.c - freestanding probe: dyld shared cache discovery via raw syscalls only.
// Built to have ZERO imports from libSystem, which arm64 nonetheless requires
// the executable to declare.
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

static i64 raw3(i64 n, i64 a, i64 b, i64 c) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80"
	                 : "+r"(x0)
	                 : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
	return x0;
}

#define BSDOUT  0x2000004
#define BSDEXIT 0x2000001
#define SYS_shared_region_check_np 294

static void out(const char *s, u64 n) { raw3(BSDOUT, 1, (i64)s, (i64)n); }
static void outs(const char *s) { u64 n = 0; while (s[n]) n++; out(s, n); }
static void outhex(u64 v) {
	char b[19];
	b[0] = '0'; b[1] = 'x';
	for (int i = 0; i < 16; i++) {
		int d = (int)((v >> (60 - 4 * i)) & 0xf);
		b[2 + i] = d < 10 ? (char)('0' + d) : (char)('a' + d - 10);
	}
	b[18] = '\n';
	out(b, 19);
}
static u32 rd32(const u8 *p) { u32 v = 0; for (int i = 0; i < 4; i++) v |= (u32)p[i] << (8 * i); return v; }

static void go(void) {
	u64 base = 0;
	i64 r = raw3(0x2000000 | SYS_shared_region_check_np, (i64)&base, 0, 0);
	outs("shared_region_check_np ret = "); outhex((u64)r);
	outs("cache base                  = "); outhex(base);
	if (r == 0) {
		const u8 *h = (const u8 *)base;
		outs("magic        = "); outhex(rd32(h));
		outs("imagesOffset = "); outhex(rd32(h + 0x10));
		outs("imagesCount  = "); outhex(rd32(h + 0x14));
	}
}

__asm__(".globl _start\n"
        "_start:\n"
        "\tbl _go\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
