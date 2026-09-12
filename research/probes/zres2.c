// zres2.c - zero-import libSystem symbol resolution for macOS arm64 loaders.
//
// Layout comes from Apple's own published source, include/mach-o/dyld_cache_format.h
// (apple-oss-distributions/dyld), not from guesswork:
//     magic[16] @0x00, mappingOffset @0x10, mappingCount @0x14,
//     imagesOffset @0x1c0, imagesCount @0x1c4, sharedRegionStart @0xe0.
// dyld_cache_image_info is 32 bytes: addr(8) modTime(8) inode(8) pathFileOffset(4) pad(4).
//
// Nothing here reads a hardcoded library path. Images are found by walking the
// cache's own image array, and symbols are resolved through each image's
// LC_SYMTAB, so a library rename or relocation does not break resolution.
//
// Zero imports: raw SVC only, no libc, no compiler helpers.

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long i64;

#define MH_MAGIC_64   0xfeedfacf
#define MH_DYLIB      6
#define LC_SEGMENT_64 0x19
#define LC_SYMTAB     0x2

// Authoritative header offsets (Apple dyld_cache_format.h).
#define CACHE_IMAGES_OFFSET 0x1c0
#define CACHE_IMAGES_COUNT  0x1c4
#define CACHE_REGION_START  0x0e0
#define CACHE_REGION_SIZE   0x0e8
#define IMAGE_INFO_SIZE     32

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
static int eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int has(const char *s, const char *sub) {
	for (; *s; s++) { const char *a = s, *b = sub; while (*a && *a == *b) { a++; b++; } if (!*b) return 1; }
	return 0;
}
static u64 slen(const char *s) { u64 n = 0; while (s[n]) n++; return n; }
static void wn(const char *s, u64 n) { svc6(0x2000004, 2, (i64)s, (i64)n, 0, 0, 0); }
static void ws(const char *s) { wn(s, slen(s)); }
static void wh(u64 v) {
	char b[32];
	b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}
	b[18]='\n';
	wn(b, 19);
}
static void wd(u64 v) {
	char b[32];
	int i = 30;
	b[i--] = '\n';
	if (!v) b[i--] = '0';
	while (v) { b[i--] = (char)('0' + v % 10); v /= 10; }
	int start = i + 1;
	wn(b + start, (u64)(31 - start));
}

static u64 shared_cache(void) {
	u64 base = 0;
	return svc6(0x2000000 | 294, (i64)&base, 0, 0, 0, 0, 0) == 0 ? base : 0;
}

// Resolve `name` in the image at `img` via its own LC_SYMTAB.
// Mach-O symbols carry a leading underscore for C names (getpid -> _getpid),
// so we match "_name" rather than "name".
static u64 image_symbol(u64 img, const char *name) {
	const u8 *h = (const u8 *)img;
	if (r32(h) != MH_MAGIC_64) return 0;
	u32 ncmds = r32(h + 16);
	if (!ncmds || ncmds > 8192) return 0;
	char want[128];
	{
		u64 i = 0;
		want[i++] = '_';
		while (name[i - 1] && i < sizeof(want) - 1) { want[i] = name[i - 1]; i++; }
		want[i] = 0;
	}
	const u8 *cmd = h + 32;
	u64 textvm = 0, leditvm = 0, leditfo = 0;
	u32 symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == LC_SEGMENT_64) {
			const char *seg = (const char *)(cmd + 8);
			if (eq(seg, "__TEXT")) textvm = r64(cmd + 24);
			else if (eq(seg, "__LINKEDIT")) { leditvm = r64(cmd + 24); leditfo = r64(cmd + 40); }
		} else if (c == LC_SYMTAB) {
			symoff = r32(cmd + 8); nsyms = r32(cmd + 12);
			stroff = r32(cmd + 16); strsize = r32(cmd + 20);
		}
		cmd += cs;
	}
	if (!nsyms || !leditvm || !textvm) return 0;
	// In the shared cache, LC_SYMTAB offsets are file offsets into the cache and
	// are reached through __LINKEDIT's vmaddr. n_value is an unslid vmaddr.
	i64 slide = (i64)img - (i64)textvm;
	i64 delta = (i64)(leditvm + slide) - (i64)leditfo;
	const u8 *st = (const u8 *)((i64)stroff + delta);
	const u8 *sm = (const u8 *)((i64)symoff + delta);
	for (u32 i = 0; i < nsyms; i++) {
		const u8 *e = sm + (u64)i * 16;              // nlist_64
		u32 strx = r32(e);
		if (strx >= strsize) continue;
		// n_type at +4. Accept only defined symbols in a section:
		// reject N_UNDEF (0x0) and debug/stab entries, which carry n_value 0
		// and would otherwise resolve to the image base.
		u8 ntype = e[4];
		if (ntype & 0xe0) continue;                  // N_STAB
		if ((ntype & 0x0e) != 0x0e) continue;        // must be N_SECT
		if (eq((const char *)(st + strx), want)) return r64(e + 8) + (u64)slide;
	}
	return 0;
}

// Walk the cache's image array (authoritative offsets) and resolve a symbol.
// `in_path` optionally restricts to images whose path contains that substring;
// pass 0 to identify purely by content.
static u64 find_symbol(const char *name, const char *in_path, int verbose, u64 *out_img) {
	u64 cache = shared_cache();
	if (!cache) return 0;
	const u8 *hdr = (const u8 *)cache;
	u32 ioff = r32(hdr + CACHE_IMAGES_OFFSET);
	u32 icnt = r32(hdr + CACHE_IMAGES_COUNT);
	if (!ioff || !icnt || icnt > 100000) return 0;
	// Image-info entries hold UNSLID vmaddrs. The runtime slide is the distance
	// between where the cache is mapped and its build-time sharedRegionStart.
	u64 region_start = r64(hdr + CACHE_REGION_START);
	i64 slide = (i64)cache - (i64)region_start;
	if (verbose) {
		ws("  cache        = "); wh(cache);
		ws("  regionStart  = "); wh(region_start);
		ws("  slide        = "); wh((u64)slide);
		ws("  imagesOffset = "); wd(ioff);
		ws("  imagesCount  = "); wd(icnt);
	}
	// The image-info array is a file offset from the cache base.
	const u8 *arr = (const u8 *)(cache + ioff);
	for (u32 i = 0; i < icnt; i++) {
		const u8 *e = arr + (u64)i * IMAGE_INFO_SIZE;
		u64 addr_unslid = r64(e);
		u32 pfo = r32(e + 24);
		if (!addr_unslid || !pfo) continue;
		u64 addr = addr_unslid + (u64)slide;
		const char *p = (const char *)(cache + pfo);
		if (in_path && !has(p, in_path)) continue;
		if (r32((const u8 *)addr) != MH_MAGIC_64) continue;
		if (r32((const u8 *)addr + 12) != MH_DYLIB) continue;
		u64 v = image_symbol(addr, name);
		if (v) { if (out_img) *out_img = addr; return v; }
	}
	return 0;
}

void cmain(i64 argc) {
	(void)argc;
	ws("zero-import libSystem resolution\n");
	ws("(layout from apple-oss-distributions/dyld dyld_cache_format.h)\n\n");

	u64 img = 0;
	ws("find 'getpid', no path hint (pure content):\n");
	u64 p = find_symbol("getpid", 0, 1, &img);
	ws("  getpid -> "); wh(p);
	if (img) { ws("  found in image at "); wh(img); }

	ws("\nresolving the loader's Syslib set:\n");
	const char *want[] = {
		"fork","pipe","clock_gettime","nanosleep","mmap",
		"pthread_jit_write_protect_supported_np","pthread_jit_write_protect_np",
		"sys_icache_invalidate","pthread_create","pthread_exit","pthread_kill",
		"pthread_sigmask","pthread_setname_np","dispatch_semaphore_create",
		"dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime",
		"pthread_self","raise","pthread_join","pthread_yield_np",
		"pthread_attr_init","pthread_attr_destroy","pthread_attr_setstacksize",
		"pthread_attr_setguardsize","exit","close","munmap","openat","write",
		"read","sigaction","pselect","mprotect","sigaltstack","getentropy",
		"sem_open","sem_unlink","sem_close","sem_post","sem_wait","sem_trywait",
		"getrlimit","setrlimit","dlopen","dlsym","dlclose","dlerror",
		"pthread_cpu_number_np","sysctl","sysctlbyname","sysctlnametomib",
	};
	u64 missing = 0, total = 0;
	for (unsigned i = 0; i < sizeof(want)/sizeof(want[0]); i++) {
		total++;
		u64 v = find_symbol(want[i], 0, 0, 0);
		if (!v) { missing++; ws("  MISSING "); ws(want[i]); ws("\n"); }
	}
	ws("\n  resolved "); wd(total - missing); ws(" of "); wd(total);
	ws("  (missing: "); wd(missing); ws(")\n");
}
__asm__(".globl _main\n"
        "_main:\n"
        "\tldr x0, [sp]\n"
        "\tbl _cmain\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
