// apexdemo.c: maximal Option-A artifact.
//
// A Mach-O arm64 executable that
//   * contains ZERO dylib load commands (no libSystem name anywhere),
//   * is built with minos 11.0 (below dyld's spring-2025 enforcement epoch),
//   * uses raw `svc #0x80` for all I/O, and
//   * discovers libSystem's exports BY CONTENT (export trie walk over the
//     shared cache image table) and calls them through function pointers.
//
// This file deliberately references no libSystem symbol at link time, so the
// binary has no undefined symbols and no dylib install-name dependency.
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8; typedef int64_t i64;

// ---- raw syscalls (BSD class = 0x2000000) ----
static inline long sys3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0  __asm__("x0")  = a;
    register long x1  __asm__("x1")  = b;
    register long x2  __asm__("x2")  = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory", "cc");
    return x0;
}
#define SYS_write 4
#define SYS_exit  1
#define SYS_shared_region_check_np 294
#define SYS_getpid 20

static long raw_write(int fd, const void *buf, size_t n) { return sys3(0x2000000 | SYS_write, fd, (long)buf, (long)n); }
static void raw_exit(int c) { sys3(0x2000000 | SYS_exit, c, 0, 0); for(;;){} }
static int  raw_cache_base(u64 *out) { return (int)sys3(0x2000000 | SYS_shared_region_check_np, (long)out, 0, 0); }
static int  raw_getpid(void) { return (int)sys3(0x2000000 | SYS_getpid, 0, 0, 0); }

static void out(const char *s, u64 n) { raw_write(1, s, n); }
static u64 slen(const char *s) { u64 n = 0; while (s[n]) n++; return n; }
static void outs(const char *s) { out(s, slen(s)); }
static void outhex(u64 v) {
    char b[19]; b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 16; i++) { int d = (v >> ((15 - i) * 4)) & 0xf; b[2+i] = d < 10 ? '0'+d : 'a'+d-10; }
    b[18] = ' '; out(b, 19);
}
static void outdec(long v) {
    if (v < 0) { out("-", 1); v = -v; }
    char b[24]; int i = 23; b[i] = ' ';
    if (!v) b[--i] = '0';
    while (v) { b[--i] = '0' + v % 10; v /= 10; }
    out(b + i, 24 - i);
}

// ---- shared cache layout (validated constants) ----
#define CH_SR_START   0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { u64 address, modTime, inode; u32 pathFileOffset, pad; } ImgInfo;

static u64 uleb(const u8 **p, const u8 *end, int *ok) {
    u64 r = 0; int sh = 0;
    while (*p < end) { u8 b = *(*p)++; r |= (u64)(b & 0x7f) << sh; if (!(b & 0x80)) return r; sh += 7; if (sh > 63) break; }
    *ok = 0; return 0;
}

// Walk an export trie for `name` (leading underscore included).
// Returns: 1 = address (off = vm offset), 2 = reexport (ord/name), 3 = absolute.
static int trieLookup(const u8 *trie, u64 size, const char *name,
                      u64 *off, u64 *ord, char *reName, u64 *absAddr) {
    if (!trie || size < 4) return 0;
    const u8 *end = trie + size, *node = trie; const char *s = name;
    for (int d = 0; d < 128; d++) {
        int ok = 1;
        const u8 *q = node;
        u64 ts = uleb(&q, end, &ok);
        if (!ok || q > end || ts > (u64)(end - q)) return 0;
        const u8 *term = q; q += ts;
        if (*s == 0 && ts) {
            const u8 *t = term;
            u64 flags = uleb(&t, term + ts, &ok);
            if (!ok) return 0;
            if (flags & 0x08) {
                *ord = uleb(&t, term + ts, &ok);
                u64 n = 0; while (t < term + ts && *t && n < 255) reName[n++] = *t++;
                reName[n] = 0;
                return 2;
            }
            if (flags & 0x10) { *absAddr = uleb(&t, term + ts, &ok); return 3; }
            *off = uleb(&t, term + ts, &ok); return 1;
        }
        u64 cc = uleb(&q, end, &ok);
        if (!ok || cc > 8192) return 0;
        const u8 *found = 0;
        for (u64 i = 0; i < cc; i++) {
            const u8 *e = q; while (e < end && *e) e++;
            if (e >= end) return 0;
            u64 el = (u64)(e - q);
            const u8 *ed = e + 1;
            u64 co = uleb(&ed, end, &ok);
            if (!ok) return 0;
            int same = 1; for (u64 k = 0; k < el; k++) if (s[k] != q[k]) { same = 0; break; }
            if (same) { found = trie + co; s += el; break; }
            q = ed;
        }
        if (!found) return 0;
        node = found;
    }
    return 0;
}

// Returns the unslid base address of the image exporting `sym`, or 0.
// Also returns the live address of the symbol when it is a direct export.
static u64 g_cache;
static const ImgInfo *g_imgs;
static u32 g_nimgs;

static void findExportInfo(const u8 *mh, u32 *expOff, u32 *expSize, u64 *linkVm, u64 *linkFileOff) {
    const u8 *p = mh; // mach_header_64: 4+4+4+4+4+4 = 24 bytes
    u32 ncmds = *(const u32 *)(p + 16), sizeofcmds = *(const u32 *)(p + 20);
    const u8 *lc = p + 32;
    *expOff = *expSize = 0; *linkVm = *linkFileOff = 0;
    for (u32 i = 0, off = 0; i < ncmds && off + 8 <= sizeofcmds; i++) {
        u32 cmd = *(const u32 *)(lc + off), sz = *(const u32 *)(lc + off + 4);
        if (sz < 8 || off + sz > sizeofcmds) break;
        if (cmd == 0x19) { // LC_SEGMENT_64
            const char *seg = (const char *)(lc + off + 8);
            if (seg[0]=='_'&&seg[1]=='_'&&seg[2]=='L'&&seg[3]=='I'&&seg[4]=='N'&&seg[5]=='K'&&seg[6]=='E'&&seg[7]=='D'&&seg[8]=='I'&&seg[9]=='T'&&seg[10]==0) {
                *linkVm = *(const u64 *)(lc + off + 24);
                *linkFileOff = *(const u64 *)(lc + off + 40);
            }
        } else if (cmd == 0x80000033) { // LC_DYLD_EXPORTS_TRIE
            *expOff = *(const u32 *)(lc + off + 8);
            *expSize = *(const u32 *)(lc + off + 12);
        } else if (cmd == 0x80000022 && !*expSize) { // LC_DYLD_INFO_ONLY
            *expOff = *(const u32 *)(lc + off + 40);
            *expSize = *(const u32 *)(lc + off + 44);
        }
        off += sz;
    }
}

// Resolve `sym` (plain name, no underscore) by content. Returns live address.
static u64 resolveByContent(const char *sym, char *outPath, u64 *outBase) {
    char name[256]; int k = 0; name[k++] = '_';
    for (int i = 0; sym[i] && k < 255; i++) name[k++] = sym[i];
    name[k] = 0;
    u64 slide = g_cache - *(const u64 *)((const u8 *)g_cache + CH_SR_START);
    u32 io = *(const u32 *)((const u8 *)g_cache + CH_IMAGES_OFF);
    u32 ic = *(const u32 *)((const u8 *)g_cache + CH_IMAGES_CNT);
    const ImgInfo *ii = (const ImgInfo *)((const u8 *)g_cache + io);
    for (u32 i = 0; i < ic; i++) {
        const u8 *mh = (const u8 *)(ii[i].address + slide);
        if (*(const u32 *)mh != 0xfeedfacf) continue;
        u32 eo, es; u64 lv, lf;
        findExportInfo(mh, &eo, &es, &lv, &lf);
        if (!es || !lv) continue;
        const u8 *linkLive = (const u8 *)(lv + slide);
        const u8 *trie = linkLive + (i64)eo - (i64)lf;
        u64 off = 0, ord = 0, absA = 0; char rn[256]; rn[0] = 0;
        int r = trieLookup(trie, es, name, &off, &ord, rn, &absA);
        if (r == 1) {
            if (outPath) { const char *p = (const char *)((const u8 *)g_cache + ii[i].pathFileOffset); int j=0; while (p[j] && j<255) { outPath[j]=p[j]; j++; } outPath[j]=0; }
            if (outBase) *outBase = off;
            return ii[i].address + slide + off;
        }
    }
    return 0;
}

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv; (void)envp;
    outs("apexdemo: alive with ZERO dylib load commands\n");

    u64 base = 0;
    int r = raw_cache_base(&base);
    outs("raw syscall 294 -> rc="); outdec(r); outs(" cacheBase="); outhex(base); outs("\n");
    g_cache = base;
    if (!base) { outs("no shared cache\n"); raw_exit(1); }

    // Discover libSystem-family exports purely by content.
    const char *syms[] = { "getpid", "write", "mmap", "pthread_create", "getentropy", "dlopen", "sysctl" };
    for (int i = 0; i < 7; i++) {
        char path[256]; u64 off = 0;
        u64 a = resolveByContent(syms[i], path, &off);
        outs("  "); outs(syms[i]);
        outs(a ? " -> " : " -> NOT FOUND ");
        if (a) { outhex(a); outs("  in "); outs(path); }
        outs("\n");
    }

    // Prove a discovered pointer actually calls: getpid via content.
    char p2[256]; u64 off2 = 0;
    u64 getpidAddr = resolveByContent("getpid", p2, &off2);
    if (getpidAddr) {
        int (*host_getpid)(void) = (int (*)(void))getpidAddr;
        int pid = host_getpid();
        outs("host getpid() via content-discovered pointer = "); outdec(pid); outs("\n");
        outs("raw getpid() for comparison                = "); outdec(raw_getpid()); outs("\n");
    }
    outs("done\n");
    raw_exit(0);
    return 0;
}
