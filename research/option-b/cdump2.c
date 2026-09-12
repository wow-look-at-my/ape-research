#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
int main(void){
  uint64_t base=0x188830000ULL;
  unsigned char*b=(unsigned char*)base;
  printf("magic=%.16s\n",b);
  uint32_t mappingOffset=*(uint32_t*)(b+16);
  uint32_t mappingCount=*(uint32_t*)(b+20);
  uint32_t imagesOffsetOld=*(uint32_t*)(b+24);
  uint32_t imagesCountOld=*(uint32_t*)(b+28);
  uint64_t dyldBase=*(uint64_t*)(b+32);
  uint64_t uuid0=*(uint64_t*)(b+88),uuid1=*(uint64_t*)(b+96);
  uint64_t cacheType=*(uint64_t*)(b+104);
  uint32_t branchPoolsOffset=*(uint32_t*)(b+112);
  uint32_t branchPoolsCount=*(uint32_t*)(b+116);
  uint64_t accelerateInfoAddr=*(uint64_t*)(b+120);
  uint64_t accelerateInfoSize=*(uint64_t*)(b+128);
  uint64_t imagesTextOffset=*(uint64_t*)(b+136);
  uint64_t imagesTextCount=*(uint64_t*)(b+144);
  uint32_t platform=*(uint32_t*)(b+216);
  uint32_t formatVersion=*(uint32_t*)(b+220);
  printf("mappingOffset=%u mappingCount=%u imagesOld=%u/%u\n",mappingOffset,mappingCount,imagesOffsetOld,imagesCountOld);
  printf("dyldBase=%#llx uuid=%016llx%016llx cacheType=%llu\n",(unsigned long long)dyldBase,(unsigned long long)uuid0,(unsigned long long)uuid1,(unsigned long long)cacheType);
  printf("imagesTextOffset=%#llx imagesTextCount=%llu\n",(unsigned long long)imagesTextOffset,(unsigned long long)imagesTextCount);
  printf("platform=%u formatVersion=%u\n",platform,formatVersion);
  printf("branchPools %u/%u accel %#llx/%#llx\n",branchPoolsOffset,branchPoolsCount,(unsigned long long)accelerateInfoAddr,(unsigned long long)accelerateInfoSize);
  printf("--- mappings ---\n");
  for(uint32_t i=0;i<mappingCount && i<40;i++){
    unsigned char*m=b+mappingOffset+i*32;
    uint64_t addr=*(uint64_t*)(m+0),size=*(uint64_t*)(m+8),fileOff=*(uint64_t*)(m+16);
    uint32_t maxProt=*(uint32_t*)(m+24),initProt=*(uint32_t*)(m+28);
    printf("  [%u] addr=%#llx size=%#llx fileOff=%#llx prot=%x/%x\n",i,(unsigned long long)addr,(unsigned long long)size,(unsigned long long)fileOff,maxProt,initProt);
  }
  printf("--- images (text) first 5 ---\n");
  for(uint64_t i=0;i<5&&i<imagesTextCount;i++){
    unsigned char*t=b+imagesTextOffset+i*32;
    uint64_t addr=*(uint64_t*)(t+0); uint32_t pfo=*(uint32_t*)(t+24);
    printf("  [%llu] addr=%#llx path=%.70s\n",(unsigned long long)i,(unsigned long long)addr,b+pfo);
  }
  return 0;
}
