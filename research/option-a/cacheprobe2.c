// cacheprobe2: find a cached image by path substring and dump its Mach-O header + load commands.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
#define CH_SR_START   0xE0
#define CH_IMGS_TEXT_OFF 0x88
#define CH_IMGS_TEXT_CNT 0x90

typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
typedef struct { uint8_t uuid[16]; uint64_t loadAddress; uint32_t textSegmentSize; uint32_t pathOffset; } ImgTextInfo;
typedef struct { uint64_t exportsTrieAddr; uint64_t weakBindingsAddr; uint32_t exportsTrieSize; uint32_t weakBindingsSize; uint32_t dependentsStartArrayIndex, reExportsStartArrayIndex; } ImgExtra;

static const char *cmdname(uint32_t c) {
    switch (c) {
    case LC_SEGMENT_64: return "LC_SEGMENT_64";
    case LC_SYMTAB: return "LC_SYMTAB";
    case LC_DYSYMTAB: return "LC_DYSYMTAB";
    case LC_LOAD_DYLIB: return "LC_LOAD_DYLIB";
    case LC_LOAD_WEAK_DYLIB: return "LC_LOAD_WEAK_DYLIB";
    case LC_ID_DYLIB: return "LC_ID_DYLIB";
    case LC_UUID: return "LC_UUID";
    case LC_BUILD_VERSION: return "LC_BUILD_VERSION";
    case LC_DYLD_INFO: return "LC_DYLD_INFO";
    case LC_DYLD_INFO_ONLY: return "LC_DYLD_INFO_ONLY";
    case LC_DYLD_EXPORTS_TRIE: return "LC_DYLD_EXPORTS_TRIE";
    case LC_DYLD_CHAINED_FIXUPS: return "LC_DYLD_CHAINED_FIXUPS";
    case LC_FUNCTION_VARIANTS: return "LC_FUNCTION_VARIANTS";
    default: return "?";
    }
}

int main(int argc, char **argv) {
    const char *path = "/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_arm64e";
    const char *want = argc > 1 ? argv[1] : "libSystem.B.dylib";
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    const uint8_t *p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    uint32_t io = *(const uint32_t *)(p + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(p + CH_IMAGES_CNT);
    uint64_t srStart = *(const uint64_t *)(p + CH_SR_START);
    const ImgInfo *ii = (const ImgInfo *)(p + io);
    const ImgInfo *found = NULL; int idx = -1;
    for (uint32_t i = 0; i < ic; i++) {
        const char *pp = (const char *)(p + ii[i].pathFileOffset);
        if (strstr(pp, want)) { found = &ii[i]; idx = i; printf("found[%d] %s addr=0x%llx\n", i, pp, (unsigned long long)ii[i].address); }
    }
    if (!found) { printf("not found\n"); return 1; }
    // Convert unslid VM address to file offset via imagesText table.
    uint64_t imgsTextOff = *(const uint64_t *)(p + CH_IMGS_TEXT_OFF);
    uint64_t imgsTextCnt = *(const uint64_t *)(p + CH_IMGS_TEXT_CNT);
    printf("imagesTextOffset=0x%llx count=%llu sizeof(struct)=%zu\n",
           (unsigned long long)imgsTextOff, (unsigned long long)imgsTextCnt, sizeof(ImgTextInfo));
    uint64_t textOff = 0, textSz = 0;
    for (uint64_t i = 0; i < imgsTextCnt; i++) {
        const ImgTextInfo *t = (const ImgTextInfo *)(p + imgsTextOff + i * 32);
        const char *tp = (const char *)(p + t->pathOffset);
        if (strstr(tp, want)) { textOff = t->loadAddress; textSz = t->textSegmentSize; }
    }
    printf("textLoadAddress(unslid)=0x%llx textSize=0x%llx\n", (unsigned long long)textOff, (unsigned long long)textSz);
    if (!textOff) { printf("no text info\n"); return 1; }
    // The images table address IS the __TEXT load address; the imagesText
    // struct gives size. Convert unslid VM addr -> file offset via the mapping table.
    uint32_t mapOff = *(const uint32_t *)(p + 0x10);
    uint32_t mapCnt = *(const uint32_t *)(p + 0x14);
    uint64_t fileOff = 0;
    for (uint32_t i = 0; i < mapCnt; i++) {
        const uint64_t *m = (const uint64_t *)(p + mapOff + i * 32);
        uint64_t a = m[0], s = m[1], fo = m[2];
        if (textOff >= a && textOff < a + s) fileOff = fo + (textOff - a);
    }
    printf("mapped file offset=0x%llx\n", (unsigned long long)fileOff);
    const struct mach_header_64 *mh = (const struct mach_header_64 *)(p + fileOff);
    printf("mach_header: magic=0x%x cputype=%d cpusubtype=%d filetype=%d ncmds=%u sizeofcmds=%u flags=0x%x\n",
           mh->magic, mh->cputype, mh->cpusubtype, mh->filetype, mh->ncmds, mh->sizeofcmds, mh->flags);
    const uint8_t *lc = (const uint8_t *)(mh + 1);
    for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
        const struct load_command *cmd = (const struct load_command *)(lc + off);
        printf("  [%2u] off=%4u cmd=0x%08x %-22s size=%u\n", i, off, cmd->cmd, cmdname(cmd->cmd), cmd->cmdsize);
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
            printf("       seg %-16s vmaddr=0x%llx vmsize=0x%llx fileoff=0x%llx filesize=0x%llx\n",
                   sg->segname, (unsigned long long)sg->vmaddr, (unsigned long long)sg->vmsize,
                   (unsigned long long)sg->fileoff, (unsigned long long)sg->filesize);
        }
        if (cmd->cmd == LC_DYLD_INFO_ONLY || cmd->cmd == LC_DYLD_INFO) {
            const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
            printf("       export_off=0x%x export_size=0x%x bind_off=0x%x lazy_off=0x%x\n",
                   d->export_off, d->export_size, d->bind_off, d->lazy_bind_off);
        }
        if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
            printf("       dataoff=0x%x datasize=0x%x\n", d->dataoff, d->datasize);
        }
        if (cmd->cmd == LC_SYMTAB) {
            const struct symtab_command *d = (const struct symtab_command *)cmd;
            printf("       symoff=0x%x nsyms=%u stroff=0x%x strsize=0x%x\n", d->symoff, d->nsyms, d->stroff, d->strsize);
        }
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
            if (sg->nsects && strcmp(sg->segname, "__LINKEDIT") == 0)
                printf("       LINKEDIT fileoff=0x%llx (cache file offset of linkedit)\n", (unsigned long long)sg->fileoff);
        }
        off += cmd->cmdsize;
    }
    return 0;
}
