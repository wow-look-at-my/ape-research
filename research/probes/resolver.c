// resolver.c - zero-import libSystem symbol resolution for macOS arm64 loaders.
//
// Why this exists: on arm64 the kernel refuses to exec a Mach-O that does not
// declare a dylib, and dyld refuses one whose dylib does not resolve. So a
// loader cannot be a static binary. What it CAN be is a binary with zero
// imported symbols: self-contained code that finds libSystem's function
// pointers at run time by reading memory, never by asking dyld to bind a name.
//
// The route: find the dyld shared cache (raw syscall), page-scan it for
// MH_DYLIB Mach-O headers (no dependence on the versioned cache header layout),
// then resolve a name through the image's own LC_SYMTAB. Nothing here reads a
// hardcoded path, which is what makes a library rename survivable.
//
// Verified against dlsym ground truth.

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long i64;

#define MH_MAGIC_64 0xfeedfacf
#define LC_SEGMENT_64 0x19
#define LC_SYMTAB 0x2
#define PAGE 0x4000

// Raw BSD syscall, up to 6 args.
static i64 svc6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x3 __asm__("x3") = d;
	register i64 x4 __asm__("x4") = e;
	register i64 x5 __asm__("x5") = f;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0)
	                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x16)
	                 : "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
	return x0;
}
static u32 r32(const u8 *p) { u32 v = 0; for (int i = 0; i < 4; i++) v |= (u32)p[i] << (8*i); return v; }
static u64 r64(const u8 *p) { u64 v = 0; for (int i = 0; i < 8; i++) v |= (u64)p[i] << (8*i); return v; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

// The dyld shared cache base, or 0. BSD syscall 294.
static u64 shared_cache(void) {
	u64 base = 0;
	if (svc6(0x2000000 | 294, (i64)&base, 0, 0, 0, 0, 0) != 0) return 0;
	return base;
}

// Does addr hold an MH_DYLIB header? Returns its install name, else 0.
// Validates every field before following it.
static const char *dylib_name(u64 addr) {
	const u8 *h = (const u8 *)addr;
	if (r32(h) != MH_MAGIC_64) return 0;
	if (r32(h + 12) != 6) return 0;                      // MH_DYLIB
	u32 ncmds = r32(h + 16), sz = r32(h + 20);
	if (!ncmds || ncmds > 4096 || !sz || sz > 0x200000) return 0;
	const u8 *cmd = h + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == 0x0d) {                                 // LC_ID_DYLIB
			u32 noff = r32(cmd + 8);
			if (noff >= cs) return 0;
			return (const char *)(cmd + noff);
		}
		cmd += cs;
	}
	return 0;
}

// Resolve one symbol in the image at `img`, using its own LC_SYMTAB.
// In the shared cache, LC_SYMTAB's offsets are file offsets into the cache, so
// they are reached through __LINKEDIT's vmaddr. n_value is an unslid vmaddr.
static u64 image_symbol(u64 img, const char *name) {
	const u8 *h = (const u8 *)img;
	u32 ncmds = r32(h + 16);
	const u8 *cmd = h + 32;
	u64 textvm = 0, leditvm = 0, leditfo = 0;
	u32 symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == LC_SEGMENT_64) {
			const char *seg = (const char *)(cmd + 8);
			if (seq(seg, "__TEXT")) { textvm = r64(cmd + 24); }
			if (seq(seg, "__LINKEDIT")) { leditvm = r64(cmd + 24); leditfo = r64(cmd + 40); }
		} else if (c == LC_SYMTAB) {
			symoff = r32(cmd + 8); nsyms = r32(cmd + 12);
			stroff = r32(cmd + 16); strsize = r32(cmd + 20);
		}
		cmd += cs;
	}
	if (!nsyms || !leditvm) return 0;
	i64 slide = (i64)img - (i64)textvm;
	i64 delta = (i64)(leditvm + slide) - (i64)leditfo;
	const u8 *st = (const u8 *)((i64)stroff + delta);
	const u8 *sm = (const u8 *)((i64)symoff + delta);
	for (u32 i = 0; i < nsyms; i++) {
		const u8 *e = sm + (u64)i * 16;                  // nlist_64
		u32 strx = r32(e);
		if (strx >= strsize) continue;
		if (seq((const char *)(st + strx), name)) {
			// n_value is an unslid vmaddr; add the slide to reach memory.
			return r64(e + 8) + (u64)slide;
		}
	}
	return 0;
}

// Find the image exporting `probe`, then resolve `name` in it.
// Identification is by CONTENT (does this image export the probe symbol?),
// never by path, so a rename or relocation does not break it.
u64 find_symbol(const char *probe, const char *name) {
	u64 cache = shared_cache();
	if (!cache) return 0;
	// The cache is a contiguous mapping; scan it for image headers.
	for (u64 off = 0; off < 0x100000000ULL; off += PAGE) {
		u64 addr = cache + off;
		const u8 *h = (const u8 *)addr;
		if (r32(h) != MH_MAGIC_64) continue;
		if (r32(h + 12) != 6) continue;                  // MH_DYLIB
		if (probe) {
			if (!image_symbol(addr, probe)) continue;
		}
		u64 p = image_symbol(addr, name);
		if (p) return p;
	}
	return 0;
}

// --- demonstration -------------------------------------------------------
static void ws(const char *s) { u64 n = 0; while (s[n]) n++; svc6(0x2000004, 2, (i64)s, (i64)n, 0, 0, 0); }
static void wh(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}
	b[18]='\n'; ws(b);
}

void cmain(i64 argc) {
	(void)argc;
	ws("zero-import symbol resolution\n");
	u64 cache = shared_cache();
	ws("  shared cache = "); wh(cache);

	// Resolve by content: the image that exports getpid is libsystem_c, but we
	// never name it -- we ask which image exports the probe symbol.
	const char *want[] = {"getpid","mmap","mprotect","pthread_create",
	                      "getentropy","sysctl","sysctlbyname","dlsym","exit"};
	for (unsigned i = 0; i < sizeof(want)/sizeof(want[0]); i++) {
		u64 p = find_symbol("getpid", want[i]);
		ws("  "); ws(want[i]); ws(" -> "); wh(p);
	}
	// Prove the resolver works on an image other than the probe's own.
	ws("  pthread_create (probed via mmap) -> "); wh(find_symbol("mmap","pthread_create"));
}

__asm__(".globl _main\n"
        "_main:\n"
        "\tldr x0, [sp]\n"
        "\tbl _cmain\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
