// triedump: parse the export trie of a Mach-O file on disk and dump symbols.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

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

static void walk(const uint8_t *trie, const uint8_t *end, uint64_t noff,
                 char *prefix, size_t plen, int *count) {
    if (*count > 200000 || noff >= (uint64_t)(end - trie)) return;
    const uint8_t *node = trie + noff;
    int ok = 1;
    const uint8_t *q = node;
    uint64_t termSize = uleb(&q, end, &ok);
    if (!ok || q + termSize > end) return;
    const uint8_t *term = q;
    q += termSize;
    if (termSize) {
        const uint8_t *t = term;
        uint64_t flags = uleb(&t, term + termSize, &ok);
        if (ok && (flags & 0x08)) {
            uint64_t ord = uleb(&t, term + termSize, &ok);
            printf("REEXPORT %s (ordinal %llu, name '%s')\n", prefix, (unsigned long long)ord, (const char *)t);
        } else if (ok) {
            uint64_t addr = uleb(&t, term + termSize, &ok);
            printf("EXPORT   %-40s = 0x%llx flags=0x%llx\n", prefix, (unsigned long long)addr, (unsigned long long)flags);
        }
        (*count)++;
    }
    uint64_t cc = uleb(&q, end, &ok);
    if (!ok) return;
    for (uint64_t i = 0; i < cc; i++) {
        const uint8_t *e = q; while (e < end && *e) e++;
        if (e >= end) return;
        size_t elen = (size_t)(e - q);
        const uint8_t *ed = e + 1;
        uint64_t co = uleb(&ed, end, &ok);
        if (!ok) return;
        if (plen + elen < 60000) {
            memcpy(prefix + plen, q, elen); prefix[plen + elen] = 0;
            walk(trie, end, co, prefix, plen + elen, count);
        }
        q = ed;
    }
}

static void dumpTrie(const uint8_t *trie, uint64_t size) {
    const uint8_t *end = trie + size;
    char prefix[65536]; prefix[0] = 0;
    int count = 0;
    walk(trie, end, 0, prefix, 0, &count);
    printf("(%d symbols)\n", count);
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: triedump FILE\n"); return 1; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    const uint8_t *p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    const struct mach_header_64 *mh = (const struct mach_header_64 *)p;
    printf("magic=0x%x ncmds=%u\n", mh->magic, mh->ncmds);
    const uint8_t *lc = (const uint8_t *)(mh + 1);
    for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
        const struct load_command *cmd = (const struct load_command *)(lc + off);
        if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
            printf("LC_DYLD_EXPORTS_TRIE off=0x%x size=0x%x\n", d->dataoff, d->datasize);
            dumpTrie(p + d->dataoff, d->datasize);
        } else if (cmd->cmd == LC_DYLD_INFO_ONLY || cmd->cmd == LC_DYLD_INFO) {
            const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
            printf("LC_DYLD_INFO export_off=0x%x export_size=0x%x\n", d->export_off, d->export_size);
            if (d->export_size) dumpTrie(p + d->export_off, d->export_size);
        }
        off += cmd->cmdsize;
    }
    return 0;
}
