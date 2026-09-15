#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define TARGET_OS_SIMULATOR 0
#define TARGET_OS_EXCLAVEKIT 0
#include "../dyld/include/mach-o/dyld_cache_format.h"
int main(void){
  printf("imagesOffset=0x%zx\n", offsetof(struct dyld_cache_header, imagesOffset));
  printf("imagesCount=0x%zx\n", offsetof(struct dyld_cache_header, imagesCount));
  printf("subCacheArrayOffset=0x%zx\n", offsetof(struct dyld_cache_header, subCacheArrayOffset));
  printf("subCacheArrayCount=0x%zx\n", offsetof(struct dyld_cache_header, subCacheArrayCount));
  printf("mappingWithSlideOffset=0x%zx\n", offsetof(struct dyld_cache_header, mappingWithSlideOffset));
  printf("mappingWithSlideCount=0x%zx\n", offsetof(struct dyld_cache_header, mappingWithSlideCount));
  printf("imagesTextOffset=0x%zx\n", offsetof(struct dyld_cache_header, imagesTextOffset));
  printf("sizeof(header)=0x%zx\n", sizeof(struct dyld_cache_header));
  printf("patchInfoAddr=0x%zx\n", offsetof(struct dyld_cache_header, patchInfoAddr));
  printf("sizeof(subcache_entry)=0x%zx\n", sizeof(struct dyld_subcache_entry));
  return 0;
}
