#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <dlfcn.h>
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
  for(uint32_t i=0;i<ic;i++){
    const char*path=(const char*)(p+ii[i].pathFileOffset);
    if(!strstr(path,v[2]))continue;
    const uint8_t*mh=(const uint8_t*)(ii[i].address+slide);
    uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
    const uint8_t*lc=mh+32;uint32_t eo=0,es=0;uint64_t lv=0,lf=0;
    for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);if(cs<8||off+cs>sz)break;
      if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
      else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
      else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}off+=cs;}
    const uint8_t*trie=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf;
    const uint8_t*end=trie+es,*node=trie;const char*s=name;
    for(int d=0;d<128;d++){int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);if(!ok||ts>(uint64_t)(end-q))break;const uint8_t*term=q;q+=ts;
      if(*s==0&&ts){const uint8_t*t=term;uint64_t fl=uleb(&t,term+ts,&ok);uint64_t a1=0,a2=0;if(fl&8){a1=uleb(&t,term+ts,&ok);a2=(uint64_t)(intptr_t)t;}else{a1=uleb(&t,term+ts,&ok);if(fl&0x10)a2=uleb(&t,term+ts,&ok);}
        printf("%s: flags=0x%llx a1(off/stub)=0x%llx a2(res/ord)=0x%llx\n  base=0x%llx -> a1 live=0x%llx  a2 live=0x%llx\n",path,(unsigned long long)fl,(unsigned long long)a1,(unsigned long long)a2,(unsigned long long)ii[i].address,(unsigned long long)(ii[i].address+slide+a1),(unsigned long long)(a2<0x10000000?ii[i].address+slide+a2:a2));
        break;}
      uint64_t cc=uleb(&q,end,&ok);const uint8_t*fn=0;
      for(uint64_t x=0;x<cc;x++){const uint8_t*e=q;while(e<end&&*e)e++;size_t el=(size_t)(e-q);const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);
        if(!strncmp((const char*)q,s,el)){fn=trie+co;s+=el;break;}q=ed;}
      if(!fn)break;node=fn;}
    void*h=dlopen(v[2][0]=='/'?v[2]:NULL,RTLD_LAZY);printf("dlsym(%s)=%p\n",v[1],h?dlsym(h,v[1]):(void*)0);
    return 0;}
  return 1;}
