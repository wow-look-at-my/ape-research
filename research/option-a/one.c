#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
static uint64_t uleb(const uint8_t**p,const uint8_t*e,int*ok){uint64_t r=0;int s=0;while(*p<e){uint8_t b=*(*p)++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))return r;s+=7;if(s>63)break;}*ok=0;return 0;}
int main(int c,char**v){
  char name[256];int k=0;name[k++]='_';for(int i=0;v[1][i]&&k<255;i++)name[k++]=v[1][i];name[k]=0;
  uint64_t cb=0;syscall(294,&cb);const uint8_t*p=(const uint8_t*)cb;
  int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
  uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF),ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
  const ImgInfo*ii=(const ImgInfo*)(p+io);
  const char*path=(const char*)(p+ii[15].pathFileOffset);
  const uint8_t*mh=(const uint8_t*)(ii[15].address+slide);
  uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
  const uint8_t*lc=mh+32;uint32_t eo=0,es=0;uint64_t lv=0,lf=0;
  for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);if(cs<8||off+cs>sz)break;
    if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
    else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
    else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}off+=cs;}
  printf("%s base=0x%llx eo=0x%x es=0x%x lv=0x%llx lf=0x%llx\n",path,(unsigned long long)ii[15].address,eo,es,(unsigned long long)lv,(unsigned long long)lf);
  const uint8_t*trie=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf;
  const uint8_t*trie2=(const uint8_t*)(lv+slide)+eo;
  printf("trieA=%p trieB=%p\n",trie,trie2);
  // walk A
  for(int pick=0;pick<2;pick++){
    const uint8_t*tr=pick?trie2:trie;
    const uint8_t*end=tr+es,*node=tr;const char*s=name;int found=0;
    for(int d=0;d<128;d++){int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);if(!ok||ts>(uint64_t)(end-q))break;const uint8_t*term=q;q+=ts;
      if(*s==0&&ts){const uint8_t*t=term;uint64_t fl=uleb(&t,term+ts,&ok);printf("  pick%d FOUND flags=0x%llx bytes:",pick,(unsigned long long)fl);
        for(uint64_t b=0;b<ts;b++)printf(" %02x",term[b]);printf("\n");found=1;break;}
      uint64_t cc=uleb(&q,end,&ok);const uint8_t*fn=0;
      for(uint64_t x=0;x<cc;x++){const uint8_t*e=q;while(e<end&&*e)e++;size_t el=(size_t)(e-q);const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);
        if(!strncmp((const char*)q,s,el)){fn=tr+co;s+=el;break;}q=ed;}
      if(!fn)break;node=fn;}
    if(!found)printf("  pick%d not found\n",pick);
  }
  return 0;
}
