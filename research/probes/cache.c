// cache.c - locate libSystem by CONTENT, not by path, using the dyld shared
// cache. Raw SVC only, zero imports. This is what makes the loader survive
// Apple renaming or relocating a library.
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
static u64 r64(const u8 *p) { u64 v = 0; for (int i=0;i<8;i++) v |= (u64)p[i] << (8*i); return v; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

// A mach_header_64 of MH_DYLIB with a plausible command count.
static int looks_like_dylib(u64 addr) {
	const u8 *h = (const u8 *)addr;
	if (r32(h) != 0xfeedfacf) return 0;
	u32 ncmds = r32(h + 16), sz = r32(h + 20);
	return ncmds > 0 && ncmds < 200 && sz > 0 && sz < 0x10000;
}

// Walk the export trie for one name; return the address offset, or 0.
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
				return (flags & 3) == 0 ? a : ~0ULL;   // non-zero flags == stub/resolver
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

// Find the export trie in a Mach-O and resolve name -> absolute address.
static u64 image_lookup(u64 base, const char *name) {
	const u8 *h = (const u8 *)base;
	u32 ncmds = r32(h + 16);
	const u8 *cmd = h + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), sz = r32(cmd + 4);
		if (c == 0x80000033) {                       // LC_DYLD_EXPORTS_TRIE
			u64 off = r32(cmd + 8);
			u64 e = trie_find(h + off, name);
			if (e == 0 || e == ~0ULL) return 0;
			return base + e;
		}
		if (c == 0x22 || c == 0x80000022) {          // LC_DYLD_INFO(_ONLY)
			u64 export_off = r32(cmd + 40);
			u64 export_size = r32(cmd + 44);
			if (!export_off || !export_size) continue;
			u64 e = trie_find(h + export_off, name);
			if (e == 0 || e == ~0ULL) return 0;
			return base + e;
		}
		cmd += sz;
	}
	return 0;
}

int main(void) {
	u64 base = 0;
	i64 r = raw3(0x2000000 | 294, (i64)&base, 0, 0);   // shared_region_check_np
	if (r != 0 || !base) { ws("no shared cache\n"); return 1; }
	const u8 *h = (const u8 *)base;
	ws("cache magic = "); ws((const char *)h); ws("\n");

	// The image array offset/count live at different places depending on the
	// header revision; try the known pairs and keep whichever validates.
	struct { u32 offo, cnto; const char *label; } cand[] = {
		{0x18, 0x1C, "old 0x18/0x1C"},
		{0xE0, 0xE4, "new 0xE0/0xE4"},
	};
	for (unsigned k = 0; k < 2; k++) {
		u32 off = r32(h + cand[k].offo), cnt = r32(h + cand[k].cnto);
		ws(cand[k].label); ws(": offset="); wu(off); ws("  count="); wu(cnt);
		if (!cnt || cnt > 100000 || !off) { ws("   (implausible)\n"); continue; }
		const u8 *imgs = h + off;
		u64 first = r64(imgs);
		ws("   first image addr = "); wh(first);
		ws("   plausible dylib? "); ws(looks_like_dylib(first) ? "yes\n" : "no\n");
		if (!looks_like_dylib(first)) continue;

		ws("\n=== scanning "); wu(cnt); ws(" images for our required exports ===\n");
		int found = 0;
		for (u32 i = 0; i < cnt; i++) {
			const u8 *e = imgs + (u64)i * 32;
			u64 addr = r64(e);
			u32 pfo = r32(e + 24);
			if (!addr || !pfo) continue;
			const char *path = (const char *)(h + pfo);
			// Identify by CONTENT: this image must export these.
			u64 mmap_ = image_lookup(addr, "mmap");
			u64 pthr  = image_lookup(addr, "pthread_create");
			u64 entr  = image_lookup(addr, "getentropy");
			if (mmap_ && pthr && entr) {
				ws("  image "); wu(i); ws("  path=\""); ws(path); ws("\"\n");
				ws("    mmap           = "); wh(mmap_);
				ws("    pthread_create = "); wh(pthr);
				ws("    getentropy     = "); wh(entr);
				found++;
			}
		}
		ws("images matching by content: "); wu((u64)found); ws("\n");
		// Prove name-independence: resolve from an image whose path we ignore.
		break;
	}
	return 0;
}


