// valid.c: corrupt the cache header copy in a buffer and confirm our parser
// refuses rather than crashing. (We cannot corrupt the real cache.)
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
static int validate(const uint8_t *p, size_t avail, uint32_t *io, uint32_t *ic) {
  if (memcmp(p, "dyld_v1", 7)) return 0;
  uint32_t o = *(const uint32_t*)(p + CH_IMAGES_OFF);
  uint32_t c = *(const uint32_t*)(p + CH_IMAGES_CNT);
  if (o < 0x200 || o > 0x10000) return 0;                 // plausible table offset
  if (c == 0 || c > 100000) return 0;                      // plausible count
  if ((uint64_t)o + (uint64_t)c * 32 > avail) return 0;    // fits
  *io = o; *ic = c; return 1;
}
int main(void){
  uint64_t cb=0; syscall(294,&cb);
  const uint8_t *p=(const uint8_t*)cb;
  uint8_t *copy = malloc(0x40000); memcpy(copy, p, 0x40000);
  uint32_t io,ic;
  printf("real header   -> valid=%d io=0x%x ic=%u\n", validate(copy,0x40000,&io,&ic), io, ic);
  // corrupt io to junk
  *(uint32_t*)(copy+CH_IMAGES_OFF)=0xdeadbeef;
  printf("io=0xdeadbeef -> valid=%d (expect 0)\n", validate(copy,0x40000,&io,&ic));
  *(uint32_t*)(copy+CH_IMAGES_OFF)=0x298;
  *(uint32_t*)(copy+CH_IMAGES_CNT)=0xffffffff;
  printf("ic=0xffffffff -> valid=%d (expect 0)\n", validate(copy,0x40000,&io,&ic));
  *(uint32_t*)(copy+CH_IMAGES_CNT)=0; printf("ic=0         -> valid=%d (expect 0)\n", validate(copy,0x40000,&io,&ic));
  *(uint32_t*)(copy+CH_IMAGES_CNT)=3646; copy[0]='X';
  printf("bad magic    -> valid=%d (expect 0)\n", validate(copy,0x40000,&io,&ic));
  // moved-imagesOffset simulation: pretend a future cache uses 0x300
  memcpy(copy, p, 0x40000);
  uint32_t realIo = *(const uint32_t*)(copy+CH_IMAGES_OFF);
  printf("(if a future cache moved the field, we would need a versioned re-probe; "
         "current=%u validated fields are the guard)\n", realIo);
  return 0;
}
