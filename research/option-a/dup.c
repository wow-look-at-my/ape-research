#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <mach-o/loader.h>
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
int main(int c,char**v){
  uint64_t cb=0;syscall(294,&cb);const uint8_t*p=(const uint8_t*)cb;
  int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
  uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF),ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
  const ImgInfo*ii=(const ImgInfo*)(p+io);
  int n=0;
  for(uint32_t i=0;i<ic;i++){
    const char*path=(const char*)(p+ii[i].pathFileOffset);
    if(!strstr(path,v[1]))continue;
    const struct mach_header_64*mh=(const struct mach_header_64*)(ii[i].address+slide);
    printf("[%u] %s addr=0x%llx magic=0x%x cpusubtype=%d filetype=%d ncmds=%u fl=0x%x\n",
      i,path,(unsigned long long)ii[i].address,mh->magic,mh->cpusubtype,mh->filetype,mh->ncmds,mh->flags);
    n++;
  }
  printf("total matching '%s': %d\n",v[1],n);
  return 0;
}
