#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
#define SR 0xE0
#define IOFF 0x1C0
static uint64_t uleb(const uint8_t**p,const uint8_t*e,int*ok){uint64_t r=0;int s=0;while(*p<e){uint8_t b=*(*p)++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))return r;s+=7;if(s>63)break;}*ok=0;return 0;}
static int countTrie(const uint8_t*tr,const uint8_t*end,uint64_t noff,int d,long*c){
 if(*c>500000||d>512||noff>=(uint64_t)(end-tr))return 0;
 const uint8_t*node=tr+noff;int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);
 if(!ok||q+ts>end)return -1; q+=ts; if(ts)(*c)++;
 uint64_t cc=uleb(&q,end,&ok); if(!ok||cc>8192)return -1;
 for(uint64_t i=0;i<cc;i++){const uint8_t*e=q;while(e<end&&*e)e++;if(e>=end)return -1;const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);if(!ok)return -1;
   if(countTrie(tr,end,co,d+1,c)<0)return -1; q=ed;}
 return 0;}
int main(void){ uint64_t cb=0;syscall(294,&cb);const uint8_t*p=(const uint8_t*)cb;
 int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+SR);
 uint32_t io=*(const uint32_t*)(p+IOFF),ic=*(const uint32_t*)(p+IOFF+4);
 const ImgInfo*ii=(const ImgInfo*)(p+io);
 int ok=0,nosym=0,bad=0,notrie=0; long total=0;
 for(uint32_t i=0;i<ic;i++){
   const uint8_t*mh=(const uint8_t*)(ii[i].address+slide);
   if(*(const uint32_t*)mh!=0xfeedfacf){bad++;continue;}
   uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);if(ncmds>8192||sz>0x200000){bad++;continue;}
   const uint8_t*lc=mh+32;uint32_t eo=0,es=0;uint64_t lv=0,lf=0;
   for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);if(cs<8||off+cs>sz)break;
     if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
     else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
     else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}off+=cs;}
   if(!es||!lv){notrie++;continue;}
   const uint8_t*tr=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf;
   long c=0; if(countTrie(tr,tr+es,0,0,&c)<0){bad++;} else {ok++;total+=c; if(!c)nosym++;}
 }
 printf("images=%u parsed-ok=%d no-trie=%d bad=%d total-symbols=%ld\n",ic,ok,notrie,bad,total);
 return 0;}
