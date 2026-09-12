// scan2.c - find libSystem in the dyld shared cache WITHOUT parsing the cache
// header layout. The header is a versioned struct Apple has changed before, so
// instead we page-scan the mapped cache region for Mach-O headers. Robust to
// header-layout churn. Raw SVC only, zero imports.
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long i64;

static i64 raw3(i64 n, i64 a, i64 b, i64 c) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x16)
	                 : "x3","x4","x5","x6","x7","x8","x9","x10","x11","x12",
	                   "x13","x14","x15","x17","memory","cc");
	return x0;
}
static void w(const char *s, u64 n) { raw3(0x2000004, 2, (i64)s, (i64)n); }
static void ws(const char *s) { u64 n = 0; while (s[n]) n++; w(s, n); }
static void wh(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}
	b[18]='\n'; w(b,19);
}
static void wu(u64 v) {
	char b[21]; int i = 20; b[i--] = '\n';
	if (!v) b[i--] = '0';
	while (v) { b[i--] = (char)('0' + v % 10); v /= 10; }
	w(b + i + 1, (u64)(20 - i));
}
static u32 r32(const u8 *p) { u32 v = 0; for (int i=0;i<4;i++) v |= (u32)p[i] << (8*i); return v; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

#define MH_MAGIC_64  0xfeedfacf
#define MH_DYLIB     6
#define LC_ID_DYLIB  0x0d

// If addr holds a Mach-O MH_DYLIB header, return its install name (or 0).
static const char *dylib_name(u64 addr) {
	const u8 *h = (const u8 *)addr;
	if (r32(h) != MH_MAGIC_64) return 0;
	u32 filetype = r32(h + 12);
	if (filetype != MH_DYLIB) return 0;
	u32 ncmds = r32(h + 16), sz = r32(h + 20);
	if (!ncmds || ncmds > 4096 || !sz || sz > 0x200000) return 0;
	const u8 *cmd = h + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == LC_ID_DYLIB) {
			u32 noff = r32(cmd + 8);
			if (noff >= cs) return 0;
			return (const char *)(cmd + noff);
		}
		cmd += cs;
	}
	return 0;
}

// Walk export trie for name; returns address offset within the image, or ~0.
static u64 trie_find(const u8 *trie, const char *name) {
	const u8 *p = trie;
	for (;;) {
		u64 tsize = 0; int sh = 0;
		while (*p & 0x80) { tsize |= (u64)(*p & 0x7f) << sh; sh += 7; p++; }
		tsize |= (u64)*p << sh; p++;
		if (tsize) {
			if (!*name) {
				u64 flags = 0; sh = 0;
				while (*p & 0x80) { flags |= (u64)(*p & 0x7f) << sh; sh += 7; p++; }
				flags |= (u64)*p << sh; p++;
				u64 a = 0; sh = 0;
				while (*p & 0x80) { a |= (u64)(*p & 0x7f) << sh; sh += 7; p++; }
				a |= (u64)*p << sh;
				return (flags & 3) ? ~0ULL : a;
			}
			p += tsize;
		}
		u8 nchild = *p++;
		if (!nchild) return 0;
		for (u8 i = 0; i < nchild; i++) {
			const char *e = (const char *)p;
			u64 el = 0; while (e[el]) el++;
			const u8 *q = p + el + 1;
			u64 off = 0; sh = 0;
			while (*q & 0x80) { off |= (u64)(*q & 0x7f) << sh; sh += 7; q++; }
			off |= (u64)*q << sh;
			if (seq(e, name)) { name += el; p = trie + off; goto next; }
			p = q + 1;
		}
		return 0;
	next:;
	}
}

// In the dyld shared cache an image is mapped at its vmaddr, and the LC_*
// "dataoff" fields are FILE offsets. To reach the data we must translate a
// file offset to a vmaddr using the __LINKEDIT segment (the trie always lives
// there). Compute the delta once per image.
static u64 linkedit_delta(const u8 *h) {
	u32 ncmds = r32(h + 16);
	const u8 *cmd = h + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) break;
		if (c == 0x19) {                                  // LC_SEGMENT_64
			const char *seg = (const char *)(cmd + 8);
			if (seq(seg, "__LINKEDIT")) {
				u64 vmaddr = (u64)r32(cmd + 24) | ((u64)r32(cmd + 28) << 32);
				u64 fileoff = (u64)r32(cmd + 40) | ((u64)r32(cmd + 44) << 32);
				return vmaddr - fileoff;                  // add to a fileoff
			}
		}
		cmd += cs;
	}
	return 0;
}

static u64 lookup(u64 base, const char *name) {
	const u8 *h = (const u8 *)base;
	u64 delta = linkedit_delta(h);
	u32 ncmds = r32(h + 16);
	const u8 *cmd = h + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == 0x80000033) {                       // LC_DYLD_EXPORTS_TRIE
			u64 off = r32(cmd + 8);                   // file offset
			u64 e = trie_find((const u8 *)(off + delta), name);
			return (e && e != ~0ULL) ? base + e : 0;
		}
		if (c == 0x22 || c == 0x80000022) {          // LC_DYLD_INFO(_ONLY)
			u64 eo = r32(cmd + 40);
			if (eo) { u64 e = trie_find((const u8 *)(eo + delta), name); if (e && e != ~0ULL) return base + e; }
		}
		cmd += cs;
	}
	return 0;
}

void cmain(void) {
	u64 cache = 0;
	i64 r = raw3(0x2000000 | 294, (i64)&cache, 0, 0);
	ws("cache ret="); wh((u64)r); ws("base="); wh(cache);
	if (r != 0 || !cache) return;
	ws("magic=\""); { char m[17]; for (int i=0;i<16;i++) m[i]=(char)((u8*)cache)[i]; m[16]=0; ws(m); } ws("\"\n");

	// The cache is a contiguous mapped region. Scan it page by page for Mach-O
	// dylib headers. No dependence on header struct layout.
	ws("scanning pages for MH_DYLIB headers...\n");
	u64 found = 0;
	// Scan a bounded window: the cache is large; cover 256 MB generously.
	for (u64 off = 0; off < 0x100000000ULL; off += 0x4000) {
		u64 addr = cache + off;
		const char *nm = dylib_name(addr);
		if (!nm) continue;
		found++;
		if (found <= 6 || seq(nm, "/usr/lib/libSystem.B.dylib")) {
			ws("  ["); wu(found); ws("] "); wh(addr); ws("     "); ws(nm); ws("\n");
			u64 p = lookup(addr, "getpid");
			ws("        getpid = "); wh(p);
			p = lookup(addr, "pthread_create");
			ws("        pthread_create = "); wh(p);
			p = lookup(addr, "mmap");
			ws("        mmap = "); wh(p);
		}
		if (found > 400) break;
	}
	ws("total MH_DYLIB headers found: "); wu(found); ws("\n");
}

__asm__(".globl _main\n"
        "_main:\n"
        "\tbl _cmain\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
