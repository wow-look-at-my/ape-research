// stubprobe2.c: for each STUB_AND_RESOLVER export in libsystem_platform, call
// the first ULEB (r1) and the second ULEB (r2) as the real function with
// correct arguments, and compare against dlsym.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <dlfcn.h>

extern int cacheInit(void);
static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r=0;int s=0;while(*p<end){uint8_t b=*(*p)++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))return r;s+=7;if(s>63)break;}*ok=0;return 0;
}
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;

// find an image index by path substring
static int findImg(const uint8_t *p, const char *sub) {
    uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF),ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
    const ImgInfo*ii=(const ImgInfo*)(p+io);
    for(uint32_t i=0;i<ic;i++) if(strstr((const char*)(p+ii[i].pathFileOffset),sub)) return (int)i;
    return -1;
}
static void getTrie(const uint8_t *p, int idx, int64_t slide, const uint8_t **trie, uint32_t *es, uint64_t *base) {
    const ImgInfo*ii=(const ImgInfo*)(p+*(const uint32_t*)(p+CH_IMAGES_OFF));
    const uint8_t*mh=(const uint8_t*)(ii[idx].address+slide);
    uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
    const uint8_t*lc=mh+32;uint32_t eo=0;uint64_t lv=0,lf=0;*es=0;
    for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);
        if(cs<8||off+cs>sz)break;
        if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
        else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);*es=*(const uint32_t*)(lc+off+12);}
        else if(cmd==0x80000022&&!*es){eo=*(const uint32_t*)(lc+off+40);*es=*(const uint32_t*)(lc+off+44);}
        off+=cs;}
    *trie=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf; *base=ii[idx].address+slide;
}
// returns 1 and fills r1,r2 for the STUB_AND_RESOLVER entry named `n` (with leading _)
static int findSR(const uint8_t *trie, uint32_t es, const char *n, uint64_t base, uint64_t *r1, uint64_t *r2) {
    const uint8_t*end=trie+es,*node=trie;const char*s=n;
    for(int d=0;d<128;d++){int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);
        if(!ok||ts>(uint64_t)(end-q))return 0;const uint8_t*term=q;q+=ts;
        if(*s==0&&ts){const uint8_t*t=term;uint64_t fl=uleb(&t,term+ts,&ok);if(!(fl&0x10))return 0;
            uint64_t u1=uleb(&t,term+ts,&ok),u2=uleb(&t,term+ts,&ok);*r1=base+u1;*r2=base+u2;return 1;}
        uint64_t cc=uleb(&q,end,&ok);const uint8_t*fn=0;
        for(uint64_t x=0;x<cc;x++){const uint8_t*e=q;while(e<end&&*e)e++;size_t el=(size_t)(e-q);const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);
            if(!strncmp((const char*)q,s,el)){fn=trie+co;s+=el;break;}q=ed;}
        if(!fn)return 0;node=fn;}
    return 0;
}

int main(void) {
    cacheInit();
    uint64_t cb=0; syscall(294,&cb);
    const uint8_t *p=(const uint8_t*)cb;
    int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
    int idx=findImg(p,"libsystem_platform.dylib");
    const uint8_t *trie; uint32_t es; uint64_t base;
    getTrie(p,idx,slide,&trie,&es,&base);
    printf("libsystem_platform idx=%d base=0x%llx trie=%p size=0x%x\n",idx,(unsigned long long)base,trie,es);

    const char *names[] = {"_platform_strcmp","_platform_strncmp","_platform_memmove","_platform_bzero","_platform_memset",NULL};
    for (int i=0; names[i]; i++) {
        uint64_t r1=0,r2=0;
        if (!findSR(trie,es,names[i],base,&r1,&r2)) { printf("%-22s no SR entry\n",names[i]); continue; }
        void *ds = dlsym(RTLD_DEFAULT, names[i]+1);
        printf("%-22s r1(firstULEB)=0x%llx r2(second)=0x%llx dlsym=%p\n",names[i],
               (unsigned long long)r1,(unsigned long long)r2,ds);
        if (!strcmp(names[i],"_platform_strcmp")) {
            int (*f1)(const char*,const char*)=(int(*)(const char*,const char*))r1;
            int (*f2)(const char*,const char*)=(int(*)(const char*,const char*))r2;
            int (*fd)(const char*,const char*)=(int(*)(const char*,const char*))ds;
            printf("   strcmp('abc','abd'): r1=%d r2=%d dlsym=%d\n", f1("abc","abd"), f2("abc","abd"), fd?fd("abc","abd"):-999);
            printf("   strcmp('abc','abc'): r1=%d r2=%d dlsym=%d\n", f1("abc","abc"), f2("abc","abc"), fd?fd("abc","abc"):-999);
        }
        if (!strcmp(names[i],"_platform_memmove")) {
            char a[32],b[32]; memset(a,0,sizeof a); memset(b,0,sizeof b); strcpy(b,"hello");
            void *(*f1)(void*,const void*,size_t)=(void*(*)(void*,const void*,size_t))r1;
            void *(*fd)(void*,const void*,size_t)=(void*(*)(void*,const void*,size_t))ds;
            f1(a,b,6); printf("   memmove r1 -> '%s'\n", a);
            if (fd) { char c[32]; memset(c,0,sizeof c); fd(c,b,6); printf("   memmove dlsym -> '%s'\n", c); }
        }
    }
    return 0;
}
