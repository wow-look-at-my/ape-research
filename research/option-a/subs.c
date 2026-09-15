// subs.c: dump each subcache header and its mapping table.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
#define CH_SUBCACHE_OFF 0x188
#define CH_SUBCACHE_CNT 0x18C

typedef struct { uint8_t uuid[16]; uint64_t cacheVMOffset; char fileSuffix[32]; } SubCacheEntry;
typedef struct { uint64_t address, size, fileOffset; uint32_t maxProt, initProt; } MapInfo;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    uint64_t cacheBase = 0;
    if ((int)syscall(294, &cacheBase) != 0 || !cacheBase) { printf("no cache\n"); return 1; }
    const uint8_t *p = (const uint8_t *)cacheBase;
    uint32_t sco = *(const uint32_t *)(p + CH_SUBCACHE_OFF);
    uint32_t scc = *(const uint32_t *)(p + CH_SUBCACHE_CNT);
    const SubCacheEntry *sc = (const SubCacheEntry *)(p + sco);
    printf("main: mappingOff=0x%x count=%u subcache count=%u\n",
           *(const uint32_t *)(p + 0x10), *(const uint32_t *)(p + 0x14), scc);
    for (uint32_t i = 0; i < 24; i++) {
        const uint8_t *cp;
        const char *suffix;
        if (i == 0) { cp = p; suffix = "(main)"; }
        else if (i - 1 < scc) { cp = p + sc[i-1].cacheVMOffset; suffix = sc[i-1].fileSuffix; }
        else break;
        if (memcmp(cp, "dyld_v1", 7) != 0) { printf("sub %-20s NO MAGIC @%p\n", suffix, cp); continue; }
        uint32_t mo = *(const uint32_t *)(cp + 0x10);
        uint32_t mc = *(const uint32_t *)(cp + 0x14);
        uint64_t srStart = *(const uint64_t *)(cp + CH_SR_START);
        printf("sub %-20s base=%p mappingOff=0x%x count=%u srStart=0x%llx\n",
               suffix, cp, mo, mc, (unsigned long long)srStart);
        for (uint32_t j = 0; j < mc && j < 8; j++) {
            const MapInfo *m = (const MapInfo *)(cp + mo + j * sizeof(MapInfo));
            printf("      map[%u] addr=0x%llx size=0x%llx fileOff=0x%llx prot=%x/%x\n", j,
                   (unsigned long long)m->address, (unsigned long long)m->size,
                   (unsigned long long)m->fileOffset, m->maxProt, m->initProt);
        }
    }
    return 0;
}
