// liveprobe: in-process shared cache introspection.
// 1. get cache base via shared_region_check_np (BSD syscall 294)
// 2. read the cache header, compute slide
// 3. find libSystem by path, deref its mach_header, walk load commands
// 4. dump __LINKEDIT, exports trie offsets
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/shared_region.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>

#define CH_MAGIC 0
#define CH_MAPPING_OFF 0x10
#define CH_MAPPING_CNT 0x14
#define CH_SR_START 0xE0
#define CH_SR_SIZE  0xE8
#define CH_MAXSLIDE 0xF0
#define CH_IMGS_TEXT_OFF 0x88
#define CH_IMGS_TEXT_CNT 0x90
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4

typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
typedef struct { uint8_t uuid[16]; uint64_t loadAddress; uint32_t textSegmentSize; uint32_t pathOffset; } ImgTextInfo;

int main(int argc, char **argv) {
    const char *want = argc > 1 ? argv[1] : "libSystem.B.dylib";
    uint64_t cacheBase = 0;
    int r = (int)syscall(294, &cacheBase);
    printf("shared_region_check_np ret=%d cacheBase=0x%llx\n", r, (unsigned long long)cacheBase);
    if (!cacheBase) { printf("no cache\n"); return 1; }
    const uint8_t *p = (const uint8_t *)cacheBase;
    printf("magic='%.16s'\n", (const char *)p);
    uint64_t srStart = *(const uint64_t *)(p + CH_SR_START);
    uint64_t srSize  = *(const uint64_t *)(p + CH_SR_SIZE);
    uint64_t maxSlide= *(const uint64_t *)(p + CH_MAXSLIDE);
    int64_t slide = (int64_t)cacheBase - (int64_t)srStart;
    printf("srStart=0x%llx srSize=0x%llx maxSlide=0x%llx slide=0x%llx\n",
           (unsigned long long)srStart, (unsigned long long)srSize,
           (unsigned long long)maxSlide, (unsigned long long)slide);
    uint32_t io = *(const uint32_t *)(p + CH_IMAGES_OFF);
    uint32_t ic = *(const uint32_t *)(p + CH_IMAGES_CNT);
    printf("imagesOffset=0x%x imagesCount=%u\n", io, ic);
    const ImgInfo *ii = (const ImgInfo *)(p + io);
    const ImgInfo *found = NULL;
    for (uint32_t i = 0; i < ic; i++) {
        const char *pp = (const char *)(p + ii[i].pathFileOffset);
        if (strstr(pp, want)) { found = &ii[i]; printf("found[%u] %s addr=0x%llx\n", i, pp, (unsigned long long)ii[i].address); }
    }
    if (!found) { printf("not found\n"); return 1; }
    const struct mach_header_64 *mh = (const struct mach_header_64 *)(intptr_t)(found->address + slide);
    printf("mh@%p magic=0x%x filetype=%d ncmds=%u sizeofcmds=%u\n", mh, mh->magic, mh->filetype, mh->ncmds, mh->sizeofcmds);
    if (mh->magic != 0xfeedfacf) { printf("BAD magic\n"); return 1; }
    const uint8_t *lc = (const uint8_t *)(mh + 1);
    uint64_t linkeditVm = 0;
    for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
        const struct load_command *cmd = (const struct load_command *)(lc + off);
        if (cmd->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sg = (const struct segment_command_64 *)cmd;
            if (strcmp(sg->segname, "__LINKEDIT") == 0) linkeditVm = sg->vmaddr;
            if (strcmp(sg->segname, "__TEXT") == 0)
                printf("  __TEXT vmaddr=0x%llx vmsize=0x%llx fileoff=0x%llx\n",
                       (unsigned long long)sg->vmaddr, (unsigned long long)sg->vmsize, (unsigned long long)sg->fileoff);
        } else if (cmd->cmd == LC_DYLD_INFO_ONLY || cmd->cmd == LC_DYLD_INFO) {
            const struct dyld_info_command *d = (const struct dyld_info_command *)cmd;
            printf("  LC_DYLD_INFO export_off=0x%x export_size=0x%x\n", d->export_off, d->export_size);
        } else if (cmd->cmd == LC_DYLD_EXPORTS_TRIE) {
            const struct linkedit_data_command *d = (const struct linkedit_data_command *)cmd;
            printf("  LC_DYLD_EXPORTS_TRIE dataoff=0x%x datasize=0x%x\n", d->dataoff, d->datasize);
        } else if (cmd->cmd == LC_SYMTAB) {
            const struct symtab_command *d = (const struct symtab_command *)cmd;
            printf("  LC_SYMTAB symoff=0x%x nsyms=%u stroff=0x%x strsize=0x%x\n", d->symoff, d->nsyms, d->stroff, d->strsize);
        } else if (cmd->cmd == LC_DYSYMTAB) {
            const struct dysymtab_command *d = (const struct dysymtab_command *)cmd;
            printf("  LC_DYSYMTAB iextdefsym=%u nextdefsym=%u\n", d->iextdefsym, d->nextdefsym);
        }
        off += cmd->cmdsize;
    }
    printf("linkeditVm(unslid)=0x%llx -> live %p\n", (unsigned long long)linkeditVm, (void *)(intptr_t)(linkeditVm + slide));
    return 0;
}
