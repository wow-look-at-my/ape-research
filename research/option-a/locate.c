// locate.c: for a given image in the live shared cache, try every plausible
// translation of an export-trie dataoff and report which one parses and
// contains a known symbol. Also dumps the subcache array.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/syscall.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
#define CH_SUBCACHE_OFF 0x188
#define CH_SUBCACHE_CNT 0x18C

typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
typedef struct { uint8_t uuid[16]; uint64_t cacheVMOffset; char fileSuffix[32]; } SubCacheEntry;

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
static int g_found;
static int trieLookup(const uint8_t *trie, uint64_t size, const char *name) {
    if (!trie || size < 4) return 0;
    const uint8_t *end = trie + size;
    const uint8_t *node = trie; const char *s = name;
    for (int depth = 0; depth < 64; depth++) {
        int ok = 1;
        const uint8_t *q = node;
        uint64_t termSize = uleb(&q, end, &ok);
        if (!ok || q > end || termSize > (uint64_t)(end - q)) return 0;
        const uint8_t *term = q; q += termSize;
        if (*s == 0 && termSize) return 1;
        uint64_t cc = uleb(&q, end, &ok);
        if (!ok || cc > 4096) return 0;
        const uint8_t *found = NULL;
        for (uint64_t i = 0; i < cc; i++) {
            const uint8_t *e = q; while (e < end && *e) e++;
            if (e >= end) return 0;
            size_t elen = (size_t)(e - q);
            const uint8_t *ed = e + 1;
            uint64_t co = uleb(&ed, end, &ok);
            if (!ok) return 0;
            if (strncmp((const char *)q, s, elen) == 0) { found = trie + co; s += elen; break; }
            q = ed;
        }
        if (!found) return 0;
        node = found;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *want = argc > 1 ? argv[1] : "libSystem.B.dylib";
    const char *sym  = argc > 2 ? argv[2] : "pthread_create";
    setvbuf(stdout, NULL, _IONBF, 0);
    uint64_t cacheBase = 0;
    if ((int)syscall(294, &cacheBase) != 0 || !cacheBase) { printf("no cache\n"); return 1; }
    const uint8_t *p = (const uint8_t *)cacheBase;
    int64_t slide = (int64_t)cacheBase - (int64_t)*(const uint64_t *)(p + CH_SR_START);
    uint32_t io = *(const uint32_t *)(p + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(p + CH_IMAGES_CNT);
    const ImgInfo *ii = (const ImgInfo *)(p + io);
    printf("cacheBase=0x%llx slide=0x%llx nImgs=%u\n", (unsigned long long)cacheBase, (unsigned long long)slide, ic);

    uint32_t sco = *(const uint32_t *)(p + CH_SUBCACHE_OFF);
    uint32_t scc = *(const uint32_t *)(p + CH_SUBCACHE_CNT);
    printf("subCacheArrayOffset=0x%x count=%u\n", sco, scc);
    const SubCacheEntry *sc = (const SubCacheEntry *)(p + sco);
    for (uint32_t i = 0; i < scc && i < 20; i++)
        printf("  sub[%u] vmOffset=0x%llx suffix='%s'\n", i,
               (unsigned long long)sc[i].cacheVMOffset, sc[i].fileSuffix);

    for (uint32_t i = 0; i < ic; i++) {
        const char *path = (const char *)(p + ii[i].pathFileOffset);
        if (!strstr(path, want)) continue;
        const struct mach_header_64 *mh = (const struct mach_header_64 *)(intptr_t)(ii[i].address + slide);
        if (mh->magic != 0xfeedfacf) continue;
        const uint8_t *lc = (const uint8_t *)(mh + 1);
        uint64_t linkVm = 0, linkFileOff = 0;
        uint32_t expOff = 0, expSize = 0;
        for (uint32_t j = 0, off = 0; j < mh->ncmds && off + 8 <= mh->sizeofcmds; j++) {
            const struct load_command *cmd = (const struct load_command *)(lc + off);
            if (cmd->cmdsize < 8 || off + cmd->cmdsize > mh->sizeofcmds) break;
            if (cmd->cmd == LC_SEGMENT_64) {
                const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
                printf("   SEG %-16s vmaddr=0x%llx vmsize=0x%llx fileoff=0x%llx filesize=0x%llx\n",
                       sg->segname, (unsigned long long)sg->vmaddr, (unsigned long long)sg->vmsize,
                       (unsigned long long)sg->fileoff, (unsigned long long)sg->filesize);
                if (strcmp(sg->segname, "__LINKEDIT") == 0) { linkVm = sg->vmaddr; linkFileOff = sg->fileoff; }
            } else if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
                const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
                expOff = d->dataoff; expSize = d->datasize;
            } else if ((cmd->cmd == LC_DYLD_INFO_ONLY) && !expSize) {
                const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
                expOff = d->export_off; expSize = d->export_size;
            }
            off += cmd->cmdsize;
        }
        printf("img[%u] %s addr=0x%llx textSize? linkVm=0x%llx linkFileOff=0x%llx expOff=0x%x expSize=0x%x\n",
               i, path, (unsigned long long)ii[i].address, (unsigned long long)linkVm,
               (unsigned long long)linkFileOff, expOff, expSize);
        const uint8_t *linkLive = (const uint8_t *)(intptr_t)(linkVm + slide);
        printf("linkLive=%p\n", linkLive);
        struct { char name[64]; const uint8_t *p; } cand[40];
        int nc = 0;
        snprintf(cand[nc].name, 64, "L+(expOff-linkFileOff)"); cand[nc++].p = linkLive + (int64_t)expOff - (int64_t)linkFileOff;
        snprintf(cand[nc].name, 64, "L+expOff"); cand[nc++].p = linkLive + expOff;
        snprintf(cand[nc].name, 64, "cacheBase+expOff"); cand[nc++].p = (const uint8_t *)cacheBase + expOff;
        snprintf(cand[nc].name, 64, "abs slide+expOff"); cand[nc++].p = (const uint8_t *)(intptr_t)(expOff + slide);
        snprintf(cand[nc].name, 64, "imgBase+expOff"); cand[nc++].p = (const uint8_t *)(intptr_t)(ii[i].address + slide) + expOff;
        // every subcache base + expOff (fileoffset == vaddr - subcacheSrStart)
        for (uint32_t k = 0; k < scc && nc < 39; k++) {
            snprintf(cand[nc].name, 64, "sub[%u](%s)+expOff", k, sc[k].fileSuffix);
            cand[nc++].p = (const uint8_t *)cacheBase + sc[k].cacheVMOffset + expOff;
        }
        for (int c = 0; c < nc; c++) {
            int hit = trieLookup(cand[c].p, expSize, sym);
            printf("   cand %-28s live=%p first8=%02x %02x %02x %02x %02x %02x %02x %02x  hit=%d\n",
                   cand[c].name, cand[c].p,
                   cand[c].p[0], cand[c].p[1], cand[c].p[2], cand[c].p[3],
                   cand[c].p[4], cand[c].p[5], cand[c].p[6], cand[c].p[7], hit);
        }
    }
    void *h = dlopen(want[0]=='/'?want:NULL, RTLD_LAZY);
    printf("dlsym('%s') = %p\n", sym, h ? dlsym(h, sym) : NULL);
    return 0;
}
