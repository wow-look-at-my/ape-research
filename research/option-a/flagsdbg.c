// flagsdbg.c: for a given symbol, show the raw terminal bytes/flags in each cache image.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;

static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r=0; int sh=0;
    while (*p<end){uint8_t b=*(*p)++; r|=(uint64_t)(b&0x7f)<<sh; if(!(b&0x80))return r; sh+=7; if(sh>63)break;}
    *ok=0; return 0;
}

int main(int argc, char**argv){
  const char*plain=argv[1];
  char name[256]; int k=0; name[k++]='_';
  for(int i=0;plain[i]&&k<255;i++)name[k++]=plain[i]; name[k]=0;
  uint64_t cb=0; syscall(294,&cb);
  const uint8_t*p=(const uint8_t*)cb;
  int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
  uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF), ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
  const ImgInfo*ii=(const ImgInfo*)(p+io);
  for(uint32_t i=0;i<ic;i++){
    const uint8_t*mh=(const uint8_t*)(ii[i].address+slide);
    if(*(const uint32_t*)mh!=0xfeedfacf)continue;
    uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
    const uint8_t*lc=mh+32; uint32_t eo=0,es=0; uint64_t lv=0,lf=0;
    for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){
      uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);
      if(cs<8||off+cs>sz)break;
      if(cmd==0x19){const char*sg=(const char*)(lc+off+8);
        if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);} }
      else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
      else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}
      off+=cs;
    }
    if(!es||!lv)continue;
    const uint8_t*trie=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf;
    const uint8_t*end=trie+es,*node=trie; const char*s=name; int found=0;
    for(int d=0;d<128;d++){
      int ok=1; const uint8_t*q=node; uint64_t ts=uleb(&q,end,&ok);
      if(!ok||q>end||ts>(uint64_t)(end-q))break;
      const uint8_t*term=q; q+=ts;
      if(*s==0&&ts){
        printf("%-46s termSize=%llu bytes:",(const char*)(p+ii[i].pathFileOffset),(unsigned long long)ts);
        for(uint64_t b=0;b<ts;b++)printf(" %02x",term[b]);
        const uint8_t*t=term; uint64_t fl=uleb(&t,term+ts,&ok);
        printf("  flags=0x%llx",(unsigned long long)fl);
        printf("  (REEXPORT=%d STUBRESOLVER=%d ABS=%d TLS=%d)",
               (fl&8)!=0,(fl&0x10)!=0,(fl&2)!=0,(fl&4)!=0);
        printf("\n");
        found=1; break;
      }
      uint64_t cc=uleb(&q,end,&ok); if(!ok||cc>8192)break;
      const uint8_t*fnd=0;
      for(uint64_t c=0;c<cc;c++){
        const uint8_t*e=q; while(e<end&&*e)e++; if(e>=end)break;
        size_t el=(size_t)(e-q); const uint8_t*ed=e+1; uint64_t co=uleb(&ed,end,&ok); if(!ok)break;
        if(!strncmp((const char*)q,s,el)){fnd=trie+co;s+=el;break;}
        q=ed;
      }
      if(!fnd)break; node=fnd;
    }
    (void)found;
  }
  return 0;
}
