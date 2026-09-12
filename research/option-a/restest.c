// restest.c: for STUB_AND_RESOLVER exports, call the dynamic resolver and see
// what it returns. This determines whether naive trie walking is sufficient.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

extern int cacheInit(void);
extern uint64_t resolveByContentEx(const char *plain, char *path, size_t sz);

// We need the raw stub+resolver pair, so re-derive here via a small local parse.
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;
typedef struct { uint64_t base; int64_t slide; const uint8_t *cache; } Ctx;
static Ctx g;
extern uint64_t g_cacheBase;
extern int64_t g_slide;
extern const uint8_t *g_cache;

static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r=0;int s=0;while(*p<end){uint8_t b=*(*p)++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))return r;s+=7;if(s>63)break;}*ok=0;return 0;
}
typedef struct { int kind; uint64_t a, b; } Hit;
static Hit lookup(const uint8_t *trie, uint64_t size, const char *name) {
    Hit h = {0,0,0};
    const uint8_t *end=trie+size,*node=trie;const char*s=name;
    for(int d=0;d<128;d++){int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);
      if(!ok||ts>(uint64_t)(end-q))break;const uint8_t*term=q;q+=ts;
      if(*s==0&&ts){const uint8_t*t=term;uint64_t fl=uleb(&t,term+ts,&ok);
        if(fl&8){h.kind=2;return h;}
        if(fl&0x10){h.kind=3;h.a=uleb(&t,term+ts,&ok);h.b=uleb(&t,term+ts,&ok);return h;}
        h.kind=1;h.a=uleb(&t,term+ts,&ok);return h;}
      uint64_t cc=uleb(&q,end,&ok);const uint8_t*fn=0;
      for(uint64_t x=0;x<cc;x++){const uint8_t*e=q;while(e<end&&*e)e++;size_t el=(size_t)(e-q);const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);
        if(!strncmp((const char*)q,s,el)){fn=trie+co;s+=el;break;}q=ed;}
      if(!fn)break;node=fn;}
    return h;
}
// find the first image whose trie has a STUB_AND_RESOLVER entry for `plain`
static int findStubResolver(const char *plain, const char **outPath, uint64_t *stub, uint64_t *res) {
    char name[256];int k=0;name[k++]='_';for(int i=0;plain[i]&&k<255;i++)name[k++]=plain[i];name[k]=0;
    const uint8_t*p=g_cache;
    uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF),ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
    const ImgInfo*ii=(const ImgInfo*)(p+io);
    for(uint32_t i=0;i<ic;i++){
      const uint8_t*mh=(const uint8_t*)(ii[i].address+g_slide);
      if(*(const uint32_t*)mh!=0xfeedfacf)continue;
      uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
      const uint8_t*lc=mh+32;uint32_t eo=0,es=0;uint64_t lv=0,lf=0;
      for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);if(cs<8||off+cs>sz)break;
        if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
        else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
        else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}off+=cs;}
      if(!es||!lv)continue;
      if (getenv("DEBUG") && strstr((const char*)(p+ii[i].pathFileOffset), "platform"))
          fprintf(stderr, "dbg %s eo=0x%x es=0x%x lv=0x%llx lf=0x%llx trie=%p name=%s\n",
                  (const char*)(p+ii[i].pathFileOffset), eo, es,
                  (unsigned long long)lv, (unsigned long long)lf, (void*)0, name);
      const uint8_t*trie=(const uint8_t*)(lv+g_slide)+(int64_t)eo-(int64_t)lf;
      Hit h=lookup(trie,es,name);
      if (getenv("DEBUG") && strstr((const char*)(p+ii[i].pathFileOffset), "platform"))
          fprintf(stderr, "dbg %s kind=%d a=0x%llx b=0x%llx\n", (const char*)(p+ii[i].pathFileOffset), h.kind, (unsigned long long)h.a, (unsigned long long)h.b);
      if(h.kind==3){*outPath=(const char*)(p+ii[i].pathFileOffset);
        *stub=ii[i].address+g_slide+h.b; *res=ii[i].address+g_slide+h.a; return 1;}
    }
    return 0;
}

int main(void) {
    cacheInit();
    const char *names[] = {"_platform_strcmp","_platform_strncmp","_platform_memmove","_platform_bzero",NULL};
    for (int i = 0; names[i]; i++) {
        const char *path=0; uint64_t stub=0,res=0;
        if (!findStubResolver(names[i], &path, &stub, &res)) {
            printf("%-18s (no STUB_AND_RESOLVER entry)\n", names[i]);
            continue;
        }
        printf("%-18s %s\n  stub=0x%llx resolver=0x%llx\n", names[i], path,
               (unsigned long long)stub, (unsigned long long)res);
        fflush(stdout);
        // Call the resolver: dyld convention is resolver() -> implementation.
        uint64_t (*resolver)(void) = (uint64_t (*)(void))res;
        uint64_t impl = resolver();
        printf("  resolver() -> 0x%llx\n", (unsigned long long)impl);
        printf("  stub bytes:"); for (int b=0;b<16;b++) printf(" %02x", ((uint8_t*)stub)[b]); printf("\n");
        printf("  impl bytes:"); for (int b=0;b<16;b++) printf(" %02x", ((uint8_t*)impl)[b]); printf("\n");
        fflush(stdout);
    }
    return 0;
}
