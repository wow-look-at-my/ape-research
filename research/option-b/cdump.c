#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
struct cache_hdr {
  char magic[16];
  uint32_t mappingOffset, mappingCount, imagesOffsetOld, imagesCountOld;
  uint64_t dyldBaseAddress, codeSignatureOffset, codeSignatureSize,
           slideInfoOffsetUnused, slideInfoSizeUnused, localSymbolsOffset, localSymbolsSize;
  uint8_t uuid[16];
  uint64_t cacheType, branchPoolsOffset, branchPoolsCount,
           accelerateInfoAddr, accelerateInfoSize, imagesTextOffset, imagesTextCount;
  uint64_t patchInfoAddr, patchInfoSize, otherImageGroupAddrUnused, otherImageGroupSizeUnused,
           progClosuresAddr, progClosuresSize, progClosuresTrieAddr, progClosuresTrieSize;
  uint32_t platform, formatVersion;
};
struct map { uint64_t address, size, fileOffset; uint32_t maxProt, initProt; };
struct imgtext { uint64_t addr, modTime, inode, pathFileOffset; uint32_t pad; };
int main(void){
  uint64_t base=0x188830000ULL;
  struct cache_hdr*h=(struct cache_hdr*)base;
  printf("magic=%.16s\n",h->magic); fflush(stdout);
  printf("mappingOffset=%#x mappingCount=%u\n",h->mappingOffset,h->mappingCount); fflush(stdout);
  printf("imagesOffsetOld=%#x imagesCountOld=%u\n",h->imagesOffsetOld,h->imagesCountOld); fflush(stdout);
  printf("dyldBaseAddress=%#llx cacheType=%llu\n",(unsigned long long)h->dyldBaseAddress,(unsigned long long)h->cacheType); fflush(stdout);
  printf("imagesTextOffset=%#llx imagesTextCount=%llu\n",(unsigned long long)h->imagesTextOffset,(unsigned long long)h->imagesTextCount); fflush(stdout);
  printf("platform=%u formatVersion=%u\n",h->platform,h->formatVersion);
  printf("--- mappings ---\n");
  for(uint32_t i=0;i<h->mappingCount && i<40;i++){
    struct map*m=(struct map*)(base+h->mappingOffset+i*sizeof(struct map));
    printf("  [%u] addr=%#llx size=%#llx fileOff=%#llx prot=%#x/%#x\n",i,
      (unsigned long long)m->address,(unsigned long long)m->size,(unsigned long long)m->fileOffset,m->maxProt,m->initProt);
  }
  printf("--- first 6 image-text entries ---\n");
  for(uint64_t i=0;i<6 && i<h->imagesTextCount;i++){
    struct imgtext*t=(struct imgtext*)(base+h->imagesTextOffset+i*sizeof(struct imgtext));
    printf("  [%llu] addr=%#llx path=%.80s\n",(unsigned long long)i,(unsigned long long)t->addr,
      (const char*)(base+t->pathFileOffset));
  }
  return 0;
}
