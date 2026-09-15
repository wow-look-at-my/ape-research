// trie3.c: in-process shared-cache symbol resolver.
// Uses shared_region_check_np to find the cache, walks the image table,
// finds an image by exported symbol by parsing its export trie, and
// compares the result to dlsym().
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4

typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;

static uint64_t cacheBase;
static int64_t  slide;
static const ImgInfo *imgs;
static uint32_t nImgs;
static const uint8_t *cacheFileBase; // == cacheBase

// ULEB128 reader with bounds checking.
static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r = 0; int shift = 0;
    while (*p < end) {
        uint8_t b = *(*p)++;
        r |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) return r;
        shift += 7;
        if (shift > 63) break;
    }
    *ok = 0; return 0;
}

// Parse export trie at `trie`, size `size`, looking for `name`.
// Returns the (unslid) address offset relative to image base, or 0.
// Also reports whether the symbol is a re-export.
static int g_reexport, g_reexportOrdinal;
static const char *g_reexportName;
static uint64_t trieLookup(const uint8_t *trie, uint64_t size, const char *name) {
    if (!trie || size < 8) return 0;
    const uint8_t *end = trie + size;
    int ok = 1;
    const uint8_t *node = trie;
    const char *s = name;
    for (int depth = 0; depth < 4096; depth++) {
        const uint8_t *q = node;
        uint64_t terminalSize = uleb(&q, end, &ok);
        if (!ok || q > end || terminalSize > (uint64_t)(end - q)) return 0;
        const uint8_t *term = q;
        q += terminalSize; // skip terminal info
        if (*s == 0 && terminalSize != 0) {
            const uint8_t *t = term;
            uint64_t flags = uleb(&t, term + terminalSize, &ok);
            if (!ok) return 0;
            if (flags & 0x08) { // EXPORT_SYMBOL_FLAGS_REEXPORT
                g_reexport = 1;
                g_reexportOrdinal = (int)uleb(&t, term + terminalSize, &ok);
                g_reexportName = (const char *)t;
                return 0;
            }
            uint64_t addr = uleb(&t, term + terminalSize, &ok);
            if (!ok) return 0;
            return addr;
        }
        // child count follows terminal payload
        uint64_t childCount = uleb(&q, end, &ok);
        if (!ok) return 0;
        const uint8_t *found = NULL;
        for (uint64_t i = 0; i < childCount; i++) {
            const uint8_t *e = q;
            while (e < end && *e) e++;
            if (e >= end) return 0;
            size_t elen = (size_t)(e - q);
            const uint8_t *ed = e + 1;
            uint64_t childOff = uleb(&ed, end, &ok);
            if (!ok) return 0;
            if (strncmp((const char *)q, s, elen) == 0) { found = trie + childOff; s += elen; break; }
            q = ed;
        }
        if (!found) return 0;
        node = found;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *sym = argc > 1 ? argv[1] : "pthread_create";
    const char *wantImg = argc > 2 ? argv[2] : NULL;
    if ((int)syscall(294, &cacheBase) != 0 || !cacheBase) { printf("no cache\n"); return 1; }
    const uint8_t *p = (const uint8_t *)cacheBase;
    slide = (int64_t)cacheBase - (int64_t)*(const uint64_t *)(p + CH_SR_START);
    uint32_t io = *(const uint32_t *)(p + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(p + CH_IMAGES_CNT);
    imgs = (const ImgInfo *)(p + io); nImgs = ic;
    printf("cacheBase=0x%llx slide=0x%llx nImgs=%u\n", (unsigned long long)cacheBase, (unsigned long long)slide, ic);

    for (uint32_t i = 0; i < ic; i++) {
        const char *path = (const char *)(p + imgs[i].pathFileOffset);
        if (wantImg && !strstr(path, wantImg)) continue;
        const struct mach_header_64 *mh = (const struct mach_header_64 *)(intptr_t)(imgs[i].address + slide);
        if (mh->magic != 0xfeedfacf || mh->ncmds > 4096 || mh->sizeofcmds > 0x100000) continue;
        const uint8_t *lc = (const uint8_t *)(mh + 1);
        uint64_t linkeditVm = 0, linkeditFileOff = 0;
        uint32_t expOff = 0, expSize = 0;
        for (uint32_t j = 0, off = 0; j < mh->ncmds && off + 8 <= mh->sizeofcmds; j++) {
            const struct load_command *cmd = (const struct load_command *)(lc + off);
            if (cmd->cmdsize < 8 || off + cmd->cmdsize > mh->sizeofcmds) break;
            if (cmd->cmd == LC_SEGMENT_64) {
                const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
                if (strcmp(sg->segname, "__LINKEDIT") == 0) { linkeditVm = sg->vmaddr; linkeditFileOff = sg->fileoff; }
            } else if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
                const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
                expOff = d->dataoff; expSize = d->datasize;
            } else if ((cmd->cmd == LC_DYLD_INFO_ONLY || cmd->cmd == LC_DYLD_INFO) && !expSize) {
                const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
                expOff = d->export_off; expSize = d->export_size;
            }
            off += cmd->cmdsize;
        }
        if (!expSize || !linkeditVm) continue;
        const uint8_t *linkeditLive = (const uint8_t *)(intptr_t)(linkeditVm + slide);
        // Candidate A: cache-file-offset into concatenated cache, translated
        // via __LINKEDIT's own file offset.
        const uint8_t *trieA = linkeditLive + (int64_t)expOff - (int64_t)linkeditFileOff;
        uint64_t aA = trieLookup(trieA, expSize, sym);
        // Candidate B: offset relative to linkedit start.
        uint64_t aB = trieLookup(linkeditLive + expOff, expSize, sym);
        // Candidate C: cacheBase + expOff (file offsets == VM offsets).
        uint64_t aC = trieLookup((const uint8_t *)cacheBase + expOff, expSize, sym);
        if (aA || aB || aC) {
            printf("img[%u] %-40s linkeditFileOff=0x%llx expOff=0x%x expSize=0x%x\n", i, path,
                   (unsigned long long)linkeditFileOff, expOff, expSize);
            if (aA) printf("   A(cache-fileoff via LINKEDIT): off=0x%llx -> live=0x%llx\n", (unsigned long long)aA, (unsigned long long)(aA + imgs[i].address + slide));
            if (aB) printf("   B(relative to linkedit):       off=0x%llx -> live=0x%llx\n", (unsigned long long)aB, (unsigned long long)(aB + imgs[i].address + slide));
            if (aC) printf("   C(cacheBase+off):              off=0x%llx -> live=0x%llx\n", (unsigned long long)aC, (unsigned long long)(aC + imgs[i].address + slide));
        }
    }
    if (wantImg) {
        void *h = dlopen(wantImg[0]=='/' ? wantImg : NULL, RTLD_LAZY);
        void *d = h ? dlsym(h, sym) : NULL;
        printf("dlsym(%s) = %p\n", sym, d);
    }
    return 0;
}
