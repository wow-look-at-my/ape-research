// Resolve a libSystem symbol with ZERO imports: walk the shared cache and
// read the image's export trie. Raw SVC only.
typedef unsigned char u8; typedef unsigned int u32; typedef unsigned long long u64; typedef long i64;
static i64 raw3(i64 n,i64 a,i64 b,i64 c){register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;register i64 x16 __asm__("x16")=n;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");return x0;}
static void w(const char*s,u64 n){raw3(0x2000004,2,(i64)s,(i64)n);}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
static u32 r32(const u8*p){u32 v=0;for(int i=0;i<4;i++)v|=(u32)p[i]<<(8*i);return v;}

// Read a NUL-terminated C string at p, compare to s.
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}

// Walk an export trie (LC_DYLD_EXPORTS_TRIE / dyld_info export_off) for a name.
static u64 trie_lookup(const u8 *trie, const char *name) {
	if (!trie) return 0;
	const u8 *p = trie + (u64)(0); // start at node 0
	// node: terminalSize (uleb), then if terminal: flags(uleb), address(uleb)
	for (;;) {
		// uleb terminal size
		u64 tsize = 0; int sh = 0;
		while (*p & 0x80) { tsize |= (u64)(*p & 0x7f) << sh; sh += 7; p++; }
		tsize |= (u64)*p << sh; p++;
		if (tsize) {
			if (!*name) {
				const u8 *t = p;
				u64 flags = 0; sh = 0;
				while (*t & 0x80) { flags |= (u64)(*t & 0x7f) << sh; sh += 7; t++; }
				flags |= (u64)*t << sh; t++;
				u64 addr = 0; sh = 0;
				while (*t & 0x80) { addr |= (u64)(*t & 0x7f) << sh; sh += 7; t++; }
				addr |= (u64)*t << sh;
				return addr;
			}
			p += tsize;
		}
		u8 nchild = *p++;
		if (!nchild) return 0;
		for (u8 i = 0; i < nchild; i++) {
			const char *edge = (const char *)p;
			u64 elen = 0; while (edge[elen]) elen++;
			u64 childoff = 0; sh = 0;
			const u8 *q = p + elen + 1;
			while (*q & 0x80) { childoff |= (u64)(*q & 0x7f) << sh; sh += 7; q++; }
			childoff |= (u64)*q << sh;
			if (seq(edge, name)) { name += elen; p = trie + childoff; goto next_node; }
			p = q + 1;
		}
		return 0;
	next_node:;
	}
}

static u64 resolve_in(const u8 *hdr, const char *name) {
	// mach_header_64: ncmds at 16, sizeofcmds at 20, cmds start at 32
	u32 ncmds = r32(hdr+16);
	const u8 *cmd = hdr + 32;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), sz = r32(cmd+4);
		if (c == 0x80000033) { // LC_DYLD_EXPORTS_TRIE
			u32 off = r32(cmd+8);
			// slide: file offset -> vmaddr (single-segment image, slide 0)
			u64 v = trie_lookup(hdr + off, name);
			if (v & 0x8000000000000000ULL) return 0; // stub
			return (u64)hdr + v;
		}
		cmd += sz;
	}
	return 0;
}

int main(void) {
	u64 base = 0;
	i64 r = raw3(0x2000000|294, (i64)&base, 0, 0);   // shared_region_check_np
	ws("cache base = "); wh(base);
	if (r != 0) { ws("no shared cache\n"); return 1; }
	const u8 *h = (const u8 *)base;
	u32 imagesOff = r32(h+0x10), imagesCount = r32(h+0x14);
	ws("magic="); wh(r32(h)); ws("images="); wh(imagesCount);
	// dyld_cache_image_info: address(8) modTime(8) inode(8) pathFileOffset(4) pad(4)
	const u8 *imgs = h + imagesOff;
	for (u32 i = 0; i < imagesCount; i++) {
		const u8 *e = imgs + (u64)i * 32;
		u64 addr = 0; for (int k=0;k<8;k++) addr |= (u64)e[k] << (8*k);
		u32 pfo = r32(e+24);
		const char *path = (const char *)(h + pfo);
		if (seq(path, "/usr/lib/libSystem.B.dylib")) {
			ws("found libSystem at "); wh(addr);
			u64 p = resolve_in((const u8 *)addr, "getpid");
			ws("getpid resolved to "); wh(p);
			p = resolve_in((const u8 *)addr, "mmap");
			ws("mmap   resolved to "); wh(p);
			p = resolve_in((const u8 *)addr, "pthread_create");
			ws("pthread_create -> "); wh(p);
		}
	}
	return 0;
}
