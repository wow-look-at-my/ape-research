// cacheprobe: parse the dyld shared cache header from a file and dump layout.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

typedef struct { uint64_t address, size, fileOffset; uint32_t maxProt, initProt; } MapInfo;
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1]
        : "/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld/dyld_shared_cache_arm64e";
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }
    const uint8_t *p = base;
    printf("file=%s size=%llu\n", path, (unsigned long long)st.st_size);
    printf("magic='%.16s'\n", (const char *)p);
    uint32_t mappingOffset = *(const uint32_t *)(p + 0x10);
    uint32_t mappingCount  = *(const uint32_t *)(p + 0x14);
    uint32_t imagesOffsetOld = *(const uint32_t *)(p + 0x18);
    uint32_t imagesCountOld  = *(const uint32_t *)(p + 0x1c);
    uint64_t dyldBaseAddress = *(const uint64_t *)(p + 0x20);
    uint64_t codeSigOff = *(const uint64_t *)(p + 0x28);
    uint64_t codeSigSize = *(const uint64_t *)(p + 0x30);
    uint64_t cacheType = *(const uint64_t *)(p + 0x68);
    uint64_t sharedRegionStart = *(const uint64_t *)(p + 0xE0);
    uint64_t sharedRegionSize  = *(const uint64_t *)(p + 0xE8);
    uint64_t maxSlide          = *(const uint64_t *)(p + 0xF0);
    uint64_t imagesTextOffset = *(const uint64_t *)(p + 0x88);
    uint64_t imagesTextCount  = *(const uint64_t *)(p + 0x90);
    uint32_t imagesOffset = *(const uint32_t *)(p + 0x1C0);
    uint32_t imagesCount  = *(const uint32_t *)(p + 0x1C4);
    uint32_t cacheSubType = *(const uint32_t *)(p + 0x1C8);
    printf("mappingOffset=0x%x mappingCount=%u\n", mappingOffset, mappingCount);
    printf("imagesOffsetOld=0x%x imagesCountOld=%u\n", imagesOffsetOld, imagesCountOld);
    printf("dyldBaseAddress=0x%llx codeSigOff=0x%llx codeSigSize=0x%llx\n",
           (unsigned long long)dyldBaseAddress, (unsigned long long)codeSigOff, (unsigned long long)codeSigSize);
    printf("cacheType=%llu cacheSubType=%u\n", (unsigned long long)cacheType, cacheSubType);
    printf("sharedRegionStart=0x%llx size=0x%llx maxSlide=0x%llx\n",
           (unsigned long long)sharedRegionStart, (unsigned long long)sharedRegionSize, (unsigned long long)maxSlide);
    printf("imagesTextOffset=0x%llx imagesTextCount=%llu\n",
           (unsigned long long)imagesTextOffset, (unsigned long long)imagesTextCount);
    printf("imagesOffset=0x%x imagesCount=%u\n", imagesOffset, imagesCount);
    // Validate
    if (imagesOffset == 0 || imagesCount == 0 || (uint64_t)imagesOffset + (uint64_t)imagesCount * 32 > st.st_size) {
        printf("IMAGES TABLE INVALID\n");
    } else {
        const ImgInfo *ii = (const ImgInfo *)(p + imagesOffset);
        int n = imagesCount < 5 ? imagesCount : 5;
        for (int i = 0; i < n; i++) {
            printf("  img[%d] addr=0x%llx pathOff=0x%x path='%s'\n", i,
                   (unsigned long long)ii[i].address, ii[i].pathFileOffset,
                   (const char *)(p + ii[i].pathFileOffset));
        }
    }
    const MapInfo *mi = (const MapInfo *)(p + mappingOffset);
    for (uint32_t i = 0; i < mappingCount && i < 40; i++)
        printf("  map[%u] addr=0x%llx size=0x%llx fileOff=0x%llx max=%x init=%x\n", i,
               (unsigned long long)mi[i].address, (unsigned long long)mi[i].size,
               (unsigned long long)mi[i].fileOffset, mi[i].maxProt, mi[i].initProt);
    return 0;
}
