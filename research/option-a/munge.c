// munge.c: patch a Mach-O executable's load commands in place.
//   munge <file> name <old> <new>   -- rewrite a dylib load-command name string
//   munge <file> filetype <n>       -- set the filetype field at offset 0x0C
//   munge <file> dropdylibs         -- remove all LC_LOAD_DYLIB/WEAK commands
//   munge <file> show               -- dump load commands
// After editing, the code signature must be refreshed: codesign -f -s - FILE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mach-o/loader.h>

#define LC_REQ_DYLD 0x80000000
#define MY_LC_LOAD_WEAK_DYLIB (0x18 | LC_REQ_DYLD)

static const char *cname(uint32_t c) {
    switch (c) {
    case LC_SEGMENT_64: return "LC_SEGMENT_64";
    case LC_SYMTAB: return "LC_SYMTAB";
    case LC_DYSYMTAB: return "LC_DYSYMTAB";
    case LC_LOAD_DYLIB: return "LC_LOAD_DYLIB";
    case MY_LC_LOAD_WEAK_DYLIB: return "LC_LOAD_WEAK_DYLIB";
    case LC_ID_DYLIB: return "LC_ID_DYLIB";
    case LC_LOAD_DYLINKER: return "LC_LOAD_DYLINKER";
    case LC_UUID: return "LC_UUID";
    case LC_BUILD_VERSION: return "LC_BUILD_VERSION";
    case LC_MAIN: return "LC_MAIN";
    case LC_DYLD_INFO_ONLY: return "LC_DYLD_INFO_ONLY";
    case LC_DYLD_EXPORTS_TRIE: return "LC_DYLD_EXPORTS_TRIE";
    case LC_CODE_SIGNATURE: return "LC_CODE_SIGNATURE";
    default: return "?";
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { printf("usage: munge FILE OP [args]\n"); return 1; }
    const char *path = argv[1], *op = argv[2];
    int fd = open(path, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    uint8_t *p = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    struct mach_header_64 *mh = (struct mach_header_64 *)p;
    if (mh->magic != 0xfeedfacf) { printf("not a thin arm64 Mach-O\n"); return 1; }
    uint8_t *lc = (uint8_t *)(mh + 1);

    if (!strcmp(op, "show")) {
        for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
            struct load_command *cmd = (struct load_command *)(lc + off);
            printf("[%2u] off=%4u %-22s size=%u", i, off, cname(cmd->cmd), cmd->cmdsize);
            if (cmd->cmd == LC_LOAD_DYLIB || cmd->cmd == MY_LC_LOAD_WEAK_DYLIB || cmd->cmd == LC_ID_DYLIB) {
                struct dylib_command *d = (struct dylib_command *)cmd;
                printf("  name='%s'", (char *)cmd + d->dylib.name.offset);
            }
            if (cmd->cmd == LC_LOAD_DYLINKER) {
                struct dylinker_command *d = (struct dylinker_command *)cmd;
                printf("  name='%s'", (char *)cmd + d->name.offset);
            }
            printf("\n");
            off += cmd->cmdsize;
        }
        printf("filetype=%u ncmds=%u sizeofcmds=%u hdrflags=0x%x\n", mh->filetype, mh->ncmds, mh->sizeofcmds, mh->flags);
        return 0;
    }
    if (!strcmp(op, "filetype")) {
        uint32_t n = (uint32_t)strtoul(argv[3], NULL, 0);
        mh->filetype = n;
        printf("set filetype=%u\n", n);
        return 0;
    }
    if (!strcmp(op, "hdrflags")) {
        uint32_t n = (uint32_t)strtoul(argv[3], NULL, 0);
        printf("hdrflags 0x%x -> 0x%x\n", mh->flags, n);
        mh->flags = n;
        return 0;
    }
    if (!strcmp(op, "name")) {
        const char *old = argv[3], *nw = argv[4];
        for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
            struct load_command *cmd = (struct load_command *)(lc + off);
            if (cmd->cmd == LC_LOAD_DYLIB || cmd->cmd == MY_LC_LOAD_WEAK_DYLIB || cmd->cmd == LC_ID_DYLIB || cmd->cmd == LC_LOAD_DYLINKER) {
                char *nm = (char *)cmd + *(uint32_t *)((uint8_t *)cmd + 8);
                if (!strcmp(nm, old)) {
                    size_t cap = cmd->cmdsize - (nm - (char *)cmd);
                    if (strlen(nw) + 1 > cap) { printf("new name too long (cap=%zu)\n", cap); return 1; }
                    memset(nm, 0, cap);
                    strcpy(nm, nw);
                    printf("renamed '%s' -> '%s'\n", old, nw);
                }
            }
            off += cmd->cmdsize;
        }
        return 0;
    }
    if (!strcmp(op, "dropdylibs")) {
        // Compact: remove LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB / LC_ID_DYLIB commands.
        uint8_t *dst = lc; uint32_t n = 0;
        uint32_t dropped = 0;
        for (uint32_t i = 0, off = 0; i < mh->ncmds && off < mh->sizeofcmds; i++) {
            struct load_command *cmd = (struct load_command *)(lc + off);
            int drop = (cmd->cmd == LC_LOAD_DYLIB || cmd->cmd == MY_LC_LOAD_WEAK_DYLIB || cmd->cmd == LC_ID_DYLIB);
            if (drop) { dropped++; }
            else { memmove(dst, cmd, cmd->cmdsize); dst += cmd->cmdsize; n++; }
            off += cmd->cmdsize;
        }
        mh->ncmds = n;
        mh->sizeofcmds -= (uint32_t)(dropped * 0); // sizes recomputed below
        // recompute sizeofcmds as sum
        uint32_t sz = 0;
        for (uint32_t i = 0, off = 0; i < n; i++) {
            struct load_command *cmd = (struct load_command *)(lc + off);
            sz += cmd->cmdsize; off += cmd->cmdsize;
        }
        mh->sizeofcmds = sz;
        printf("dropped %u dylib commands, ncmds=%u sizeofcmds=%u\n", dropped, n, sz);
        return 0;
    }
    printf("unknown op\n");
    return 1;
}
