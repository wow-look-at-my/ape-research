// cacheresolve.c: resolve a symbol to an absolute address by walking the dyld
// shared cache IMAGE TABLE and parsing each image's LC_DYLD_EXPORTS_TRIE -- no
// dlopen/dlsym, no link-time symbol dependency. Content-based discovery.
//
// Findings encoded here:
//  * header.imagesOffset is at file offset 0x1C0 (imagesCount at 0x1C4).
//    The old fields at 0x18/0x1C are UNUSED (zero on modern caches).
//  * slide = cacheBase - header.sharedRegionStart.
//  * image mach_header live address = images[i].address + slide.
//  * export trie bytes live in the __LINKEDIT subcache. The load command's
//    dataoff is a file offset within that subcache; the trie address is
//    linkeditLive + (dataoff - linkeditSegment.fileoff). We validate by
//    parsing.
//  * trie symbol names carry the leading underscore.
//  * a trie terminal may be a REEXPORT (flags & 0x08) whose implementation
//    lives in another image, named by ordinal.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/syscall.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define CH_SR_START     0xE0
#define CH_IMAGES_OFF   0x1C0
#define CH_IMAGES_CNT   0x1C4
#define CH_SUBCACHE_OFF 0x188
#define CH_SUBCACHE_CNT 0x18C

typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
typedef struct { uint8_t uuid[16]; uint64_t cacheVMOffset; char fileSuffix[32]; } SubCacheEntry;

uint64_t g_cacheBase;
int64_t  g_slide;
const uint8_t *g_cache;
static const ImgInfo *g_imgs;
uint32_t g_nImgs;

static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r = 0; int shift = 0;
    while (*p < end) {
        uint8_t b = *(*p)++;
        r |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) return r;
        shift += 7; if (shift > 63) break;
    }
    *ok = 0; return 0;
}

// Result of a trie walk.
typedef enum { TR_NOTFOUND, TR_ADDRESS, TR_REEXPORT, TR_ABS, TR_STUB_RESOLVER, TR_THREAD_LOCAL } TrieResult;
typedef struct {
    TrieResult kind;
    uint64_t   vmOffset;      // for TR_ADDRESS: offset from image base
    uint64_t   absAddress;    // for TR_ABS
    uint64_t   resolver;      // for TR_STUB_RESOLVER
    uint64_t   flags;
    uint64_t   ordinal;       // for TR_REEXPORT
    char       importName[256]; // for TR_REEXPORT (empty => same name)
} TrieHit;

// Walk export trie `trie` (size `size`) for `name` (WITH leading underscore).
static TrieHit trieLookup(const uint8_t *trie, uint64_t size, const char *name) {
    TrieHit hit; memset(&hit, 0, sizeof hit); hit.kind = TR_NOTFOUND;
    if (!trie || size < 4) return hit;
    const uint8_t *end = trie + size;
    const uint8_t *node = trie; const char *s = name;
    for (int depth = 0; depth < 256; depth++) {
        int ok = 1;
        const uint8_t *q = node;
        uint64_t termSize = uleb(&q, end, &ok);
        if (!ok || q > end || termSize > (uint64_t)(end - q)) return hit;
        const uint8_t *term = q; q += termSize;
        if (*s == 0 && termSize) {
            const uint8_t *t = term;
            uint64_t flags = uleb(&t, term + termSize, &ok);
            if (!ok) return hit;
            hit.flags = flags;
            if (flags & 0x08) { // EXPORT_SYMBOL_FLAGS_REEXPORT
                hit.kind = TR_REEXPORT;
                hit.ordinal = uleb(&t, term + termSize, &ok);
                const char *in = (const char *)t;
                size_t n = 0;
                while (t < term + termSize && *t && n < 255) { hit.importName[n++] = *t++; }
                hit.importName[n] = 0;
                (void)in;
                return hit;
            }
            if (flags & 0x10) { // EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER
                // Measured encoding: first ULEB = implementation, second = stub.
                hit.kind = TR_STUB_RESOLVER;
                hit.resolver = uleb(&t, term + termSize, &ok);
                hit.vmOffset = uleb(&t, term + termSize, &ok);
                return hit;
            }
            if (flags & 0x04) { // EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL
                hit.kind = TR_THREAD_LOCAL;
                hit.vmOffset = uleb(&t, term + termSize, &ok);
                return hit;
            }
            if (flags & 0x02) { // EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE
                hit.kind = TR_ABS;
                hit.absAddress = uleb(&t, term + termSize, &ok);
                return hit;
            }
            hit.kind = TR_ADDRESS;
            hit.vmOffset = uleb(&t, term + termSize, &ok);
            return hit;
        }
        uint64_t cc = uleb(&q, end, &ok);
        if (!ok || cc > 8192) return hit;
        const uint8_t *found = NULL;
        for (uint64_t i = 0; i < cc; i++) {
            const uint8_t *e = q; while (e < end && *e) e++;
            if (e >= end) return hit;
            size_t elen = (size_t)(e - q);
            const uint8_t *ed = e + 1;
            uint64_t co = uleb(&ed, end, &ok);
            if (!ok) return hit;
            if (strncmp((const char *)q, s, elen) == 0) { found = trie + co; s += elen; break; }
            q = ed;
        }
        if (!found) return hit;
        node = found;
    }
    return hit;
}

// Count symbols by walking whole trie; also validates the placement.
static void walkCount(const uint8_t *trie, const uint8_t *end, uint64_t noff,
                      int depth, long *count) {
    if (depth > 512 || noff >= (uint64_t)(end - trie) || *count > 3000000) return;
    const uint8_t *node = trie + noff; int ok = 1;
    const uint8_t *q = node;
    uint64_t ts = uleb(&q, end, &ok);
    if (!ok || q + ts > end) return;
    q += ts;
    if (ts) (*count)++;
    uint64_t cc = uleb(&q, end, &ok);
    if (!ok || cc > 8192) return;
    for (uint64_t i = 0; i < cc; i++) {
        const uint8_t *e = q; while (e < end && *e) e++;
        if (e >= end) return;
        const uint8_t *ed = e + 1;
        uint64_t co = uleb(&ed, end, &ok);
        if (!ok) return;
        walkCount(trie, end, co, depth + 1, count);
        q = ed;
    }
}

typedef struct {
    const char *path;
    uint64_t    base;        // unslid load address
    const uint8_t *mh;       // live mach_header
    uint32_t    expOff, expSize;
    uint64_t    linkVm, linkFileOff;
    const uint8_t *trie;     // validated trie location
    uint64_t    depCount;
    const char *depPaths[64];
} Image;

static void findExportInfo(Image *im) {
    const struct mach_header_64 *mh = (const struct mach_header_64 *)im->mh;
    const uint8_t *lc = (const uint8_t *)(mh + 1);
    im->expOff = im->expSize = 0; im->linkVm = im->linkFileOff = 0;
    for (uint32_t j = 0, off = 0; j < mh->ncmds && off + 8 <= mh->sizeofcmds; j++) {
        const struct load_command *cmd = (const struct load_command *)(lc + off);
        if (cmd->cmdsize < 8 || off + cmd->cmdsize > mh->sizeofcmds) break;
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
            if (!strcmp(sg->segname, "__LINKEDIT")) { im->linkVm = sg->vmaddr; im->linkFileOff = sg->fileoff; }
        } else if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
            im->expOff = d->dataoff; im->expSize = d->datasize;
        } else if (cmd->cmd == LC_DYLD_INFO_ONLY && !im->expSize) {
            const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
            im->expOff = d->export_off; im->expSize = d->export_size;
        } else if (((cmd->cmd == LC_LOAD_DYLIB) || (cmd->cmd == LC_REEXPORT_DYLIB) ||
                    (cmd->cmd == LC_LOAD_UPWARD_DYLIB) || (cmd->cmd == LC_LAZY_LOAD_DYLIB)) &&
                   im->depCount < 64) {
            const struct dylib_command *d = (const struct dylib_command *)cmd;
            im->depPaths[im->depCount++] = (const char *)cmd + d->dylib.name.offset;
        }
        off += cmd->cmdsize;
    }
    if (!im->expSize || !im->linkVm) return;
    const uint8_t *linkLive = (const uint8_t *)(intptr_t)(im->linkVm + g_slide);
    // Candidate placements; pick the one that parses into a plausible trie.
    struct { const uint8_t *p; } cand[3] = {
        { linkLive + (int64_t)im->expOff - (int64_t)im->linkFileOff }, // subcache file offset
        { linkLive + im->expOff },                                    // linkedit-relative
        { g_cache + im->expOff },                                     // unified cache offset
    };
    for (int c = 0; c < 3; c++) {
        long n = 0;
        // Validate: the root must parse; then count symbols (bounded).
        walkCount(cand[c].p, cand[c].p + im->expSize, 0, 0, &n);
        if (n > 0) { im->trie = cand[c].p; return; }
    }
}

static int findImageByPath(const char *sub, Image *out) {
    for (uint32_t i = 0; i < g_nImgs; i++) {
        const char *path = (const char *)(g_cache + g_imgs[i].pathFileOffset);
        if (g_imgs[i].pathFileOffset && strstr(path, sub)) {
            const uint8_t *mh = (const uint8_t *)(intptr_t)(g_imgs[i].address + g_slide);
            if (*(const uint32_t *)mh != 0xfeedfacf) continue;
            memset(out, 0, sizeof *out);
            out->path = path; out->base = g_imgs[i].address; out->mh = mh;
            findExportInfo(out);
            return 1;
        }
    }
    return 0;
}

static uint64_t resolve(const char *imgPath, const char *symPlain, int depth, int verbose);

// Resolve a name (plain, no underscore) in a specific image.
static uint64_t lookupIn(Image *im, const char *symPlain, int depth, int verbose) {
    char name[512];
    snprintf(name, sizeof name, "_%s", symPlain);
    if (!im->trie) return 0;
    TrieHit h = trieLookup(im->trie, im->expSize, name);
    if (verbose && h.kind != TR_NOTFOUND)
        printf("    trie %s in %s -> kind=%d off=0x%llx ord=%llu imp='%s'\n",
               name, im->path, h.kind, (unsigned long long)h.vmOffset,
               (unsigned long long)h.ordinal, h.importName);
    switch (h.kind) {
    case TR_ADDRESS:
    case TR_THREAD_LOCAL: return im->base + g_slide + h.vmOffset;
    case TR_STUB_RESOLVER: {
        // EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER encoding (measured, not guessed):
        //   first ULEB  -> callable implementation entry point
        //   second ULEB -> arm64e dispatch stub (NOT callable as the function)
        // dlsym() may return yet another (CPU-optimized) implementation, so an
        // address difference vs dlsym is expected; both are valid call targets.
        return im->base + g_slide + h.resolver;
    }
    case TR_ABS:     return h.absAddress;
    case TR_REEXPORT: {
        if (depth > 8) return 0;
        const char *nm = h.importName[0] ? h.importName + 1 : symPlain;
        // ordinal > 0: resolve inside that specific dependent (1-based).
        if (h.ordinal > 0 && h.ordinal <= im->depCount) {
            Image dep;
            if (findImageByPath(im->depPaths[h.ordinal - 1], &dep)) {
                uint64_t a = lookupIn(&dep, nm, depth + 1, verbose);
                if (a) return a;
            }
        }
        // ordinal == 0 or the dependent did not have it: flat-namespace scan.
        for (uint32_t i = 0; i < g_nImgs; i++) {
            const uint8_t *mh = (const uint8_t *)(intptr_t)(g_imgs[i].address + g_slide);
            if (*(const uint32_t *)mh != 0xfeedfacf) continue;
            Image t; memset(&t, 0, sizeof t);
            t.path = (const char *)(g_cache + g_imgs[i].pathFileOffset);
            t.base = g_imgs[i].address; t.mh = mh; findExportInfo(&t);
            if (!t.trie) continue;
            if (t.trie == im->trie) continue;
            uint64_t a = lookupIn(&t, nm, depth + 1, 0);
            if (a) return a;
        }
        return 0;
    }
    default: return 0;
    }
}

static uint64_t resolve(const char *imgPath, const char *symPlain, int depth, int verbose) {
    (void)depth;
    Image im;
    if (!findImageByPath(imgPath, &im)) { printf("image %s not found\n", imgPath); return 0; }
    return lookupIn(&im, symPlain, 0, verbose);
}

// Find ANY image exporting `symPlain` (following re-exports) -- content discovery.
static uint64_t resolveByContent(const char *symPlain, char *outPath, size_t outSz, int verbose) {
    for (uint32_t i = 0; i < g_nImgs; i++) {
        const uint8_t *mh = (const uint8_t *)(intptr_t)(g_imgs[i].address + g_slide);
        if (*(const uint32_t *)mh != 0xfeedfacf) continue;
        Image t; memset(&t, 0, sizeof t);
        t.path = (const char *)(g_cache + g_imgs[i].pathFileOffset);
        t.base = g_imgs[i].address; t.mh = mh; findExportInfo(&t);
        if (!t.trie) continue;
        uint64_t a = lookupIn(&t, symPlain, 0, 0);
        if (a) {
            if (outPath) snprintf(outPath, outSz, "%s", t.path);
            if (verbose) printf("    resolved in %s\n", t.path);
            return a;
        }
    }
    return 0;
}

static void dumpReexports(const char *imgPath) {
    Image im;
    if (!findImageByPath(imgPath, &im)) { printf("no image %s\n", imgPath); return; }
    printf("== reexports of %s (trie=%p expSize=0x%x deps=%llu)\n", im.path, im.trie, im.expSize, (unsigned long long)im.depCount);
    for (uint64_t d = 0; d < im.depCount; d++) printf("   dep[%llu] = %s\n", (unsigned long long)d + 1, im.depPaths[d]);
    if (!im.trie) return;
    // Walk whole trie, printing reexport terminators with their path.
    const uint8_t *end = im.trie + im.expSize;
    struct { const uint8_t *node; char pfx[256]; } stack_[512];
    int sp = 0; stack_[sp].node = im.trie; stack_[sp].pfx[0] = 0; sp++;
    int shown = 0;
    while (sp > 0 && shown < 400) {
        sp--;
        const uint8_t *node = stack_[sp].node;
        char pfx[256]; memcpy(pfx, stack_[sp].pfx, 256);
        size_t plen = strlen(pfx);
        int ok = 1; const uint8_t *q = node;
        uint64_t ts = uleb(&q, end, &ok);
        if (!ok || q + ts > end) continue;
        const uint8_t *term = q; q += ts;
        if (ts) {
            const uint8_t *t = term;
            uint64_t flags = uleb(&t, term + ts, &ok);
            if (ok && (flags & 0x08)) {
                uint64_t ord = uleb(&t, term + ts, &ok);
                printf("   REEXPORT %-40s ord=%llu name='%s'\n", pfx, (unsigned long long)ord, (const char *)t);
                shown++;
            } else if (ok && (flags & 0x10)) {
                uint64_t a = uleb(&t, term + ts, &ok);
                printf("   ABS      %-40s = 0x%llx\n", pfx, (unsigned long long)a); shown++;
            } else if (ok) {
                uint64_t a = uleb(&t, term + ts, &ok);
                printf("   EXPORT   %-40s = 0x%llx\n", pfx, (unsigned long long)a); shown++;
            }
        }
        uint64_t cc = uleb(&q, end, &ok);
        if (!ok) continue;
        for (uint64_t i = 0; i < cc && sp < 500; i++) {
            const uint8_t *e = q; while (e < end && *e) e++;
            if (e >= end) break;
            size_t elen = (size_t)(e - q);
            const uint8_t *ed = e + 1;
            uint64_t co = uleb(&ed, end, &ok);
            if (!ok) break;
            if (plen + elen < 255) {
                memcpy(stack_[sp].pfx, pfx, plen);
                memcpy(stack_[sp].pfx + plen, q, elen);
                stack_[sp].pfx[plen + elen] = 0;
                stack_[sp].node = im.trie + co;
                sp++;
            }
            q = ed;
        }
    }
}

// ---- exported entry points for scaletest.c ----
int cacheInit(void) {
    if ((int)syscall(294, &g_cacheBase) != 0 || !g_cacheBase) return 1;
    g_cache = (const uint8_t *)g_cacheBase;
    uint64_t srStart = *(const uint64_t *)(g_cache + CH_SR_START);
    g_slide = (int64_t)g_cacheBase - (int64_t)srStart;
    uint32_t io = *(const uint32_t *)(g_cache + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(g_cache + CH_IMAGES_CNT);
    g_imgs = (const ImgInfo *)(g_cache + io); g_nImgs = ic;
    return 0;
}
void cacheInfo(uint64_t *base, int64_t *slide, uint32_t *n) {
    if (base) *base = g_cacheBase; if (slide) *slide = g_slide; if (n) *n = g_nImgs;
}
uint64_t resolveByContentEx(const char *plain, char *path, size_t sz) {
    return resolveByContent(plain, path, sz, 0);
}

#ifndef NO_MAIN
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((int)syscall(294, &g_cacheBase) != 0 || !g_cacheBase) { printf("no cache\n"); return 1; }
    g_cache = (const uint8_t *)g_cacheBase;
    uint64_t srStart = *(const uint64_t *)(g_cache + CH_SR_START);
    g_slide = (int64_t)g_cacheBase - (int64_t)srStart;
    uint32_t io = *(const uint32_t *)(g_cache + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(g_cache + CH_IMAGES_CNT);
    g_imgs = (const ImgInfo *)(g_cache + io); g_nImgs = ic;
    printf("cacheBase=0x%llx slide=0x%llx imagesOffset=0x%x imagesCount=%u\n",
           (unsigned long long)g_cacheBase, (unsigned long long)g_slide, io, ic);

    if (argc > 2 && !strcmp(argv[1], "--dump")) { dumpReexports(argv[2]); return 0; }
    if (argc > 2 && !strcmp(argv[1], "--resolve-in")) {
        char path[256]; uint64_t a = resolve(argv[2], argv[3], 0, 1);
        (void)path;
        void *h = dlopen(argv[2][0]=='/'?argv[2]:NULL, RTLD_LAZY);
        void *ref = h ? dlsym(h, argv[3]) : NULL;
        printf("resolve-in %s %s -> 0x%llx dlsym=%p %s\n", argv[2], argv[3],
               (unsigned long long)a, ref, a == (uint64_t)ref ? "MATCH" : "DIFF");
        return 0;
    }

    // Resolve each requested symbol, and compare to dlsym if possible.
    int failures = 0, checked = 0;
    for (int a = 1; a < argc; a++) {
        const char *sym = argv[a];
        char imgPath[256]; imgPath[0] = 0;
        uint64_t addr = resolveByContent(sym, imgPath, sizeof imgPath, 1);
        void *ref = dlsym(RTLD_DEFAULT, sym);
        printf("  %-24s cache=0x%llx (%s)  dlsym=%p  %s\n", sym,
               (unsigned long long)addr, imgPath[0] ? imgPath : "?",
               ref, (addr && ref && addr == (uint64_t)ref) ? "MATCH" :
                    (!addr ? "NOT-FOUND-IN-CACHE" : "MISMATCH"));
        if (ref) { checked++; if (addr != (uint64_t)ref) failures++; }
    }
    printf("checked=%d mismatches=%d\n", checked, failures);
    return 0;
}
#endif // NO_MAIN
