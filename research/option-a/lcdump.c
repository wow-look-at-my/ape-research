#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
int main(int argc, char**argv){
  uint64_t cb=0; syscall(294,&cb); char* want=argv[1];
  const uint8_t*p=(const uint8_t*)cb;
  int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
  uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF), ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
  const ImgInfo*ii=(const ImgInfo*)(p+io);
  for(uint32_t i=0;i<ic;i++){
    const char*path=(const char*)(p+ii[i].pathFileOffset);
    if(!strstr(path,want))continue;
    const uint8_t*mh=(const uint8_t*)(ii[i].address+slide);
    if(*(const uint32_t*)mh!=0xfeedfacf)continue;
    uint32_t ncmds=*(const uint32_t*)(mh+16), sz=*(const uint32_t*)(mh+20);
    const uint8_t*lc=mh+32;
    printf("%s ncmds=%u\n",path,ncmds);
    for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){
      uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);
      if(cs<8||off+cs>sz)break;
      const char*nm=(cmd==0xc||cmd==0x18||cmd==0x1f||cmd==0x23||cmd==0xe)?(const char*)(lc+off+*(const uint32_t*)(lc+off+8)):"";
      if(*nm) printf("  cmd=0x%08x size=%u name='%s'\n",cmd,cs,nm);
      off+=cs;
    }
    return 0;
  }
  printf("not found\n"); return 1;
}
