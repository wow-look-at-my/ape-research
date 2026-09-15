// stubfinal.c: resolve STUB_AND_RESOLVER exports several ways and report which
// one is actually callable with correct semantics.
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

// Walk the WHOLE trie of `idx`, collecting every STUB_AND_RESOLVER leaf.
static void collect(const uint8_t *trie, const uint8_t *end, uint64_t noff,
                    char *pfx, size_t plen, uint64_t base, const char *want,
                    uint64_t *r1, uint64_t *r2, int *found) {
    if (*found || noff >= (uint64_t)(end - trie)) return;
    const uint8_t *node = trie + noff; int ok = 1;
    const uint8_t *q = node; uint64_t ts = uleb(&q, end, &ok);
    if (!ok || q + ts > end) return;
    const uint8_t *term = q; q += ts;
    if (ts) {
        const uint8_t *t = term; uint64_t fl = uleb(&t, term + ts, &ok);
        if ((fl & 0x10) && !strcmp(pfx, want)) {
            uint64_t u1 = uleb(&t, term + ts, &ok), u2 = uleb(&t, term + ts, &ok);
            *r1 = base + u1; *r2 = base + u2; *found = 1; return;
        }
    }
    uint64_t cc = uleb(&q, end, &ok);
    if (!ok) return;
    for (uint64_t i = 0; i < cc; i++) {
        const uint8_t *e = q; while (e < end && *e) e++;
        if (e >= end) return;
        size_t el = (size_t)(e - q);
        const uint8_t *ed = e + 1; uint64_t co = uleb(&ed, end, &ok);
        if (!ok) return;
        if (plen + el < 200) {
            memcpy(pfx + plen, q, el); pfx[plen + el] = 0;
            collect(trie, end, co, pfx, plen + el, base, want, r1, r2, found);
        }
        q = ed;
    }
}

int main(void) {
    cacheInit();
    uint64_t cb=0; syscall(294,&cb);
    const uint8_t *p=(const uint8_t*)cb;
    int64_t slide=(int64_t)cb-(int64_t)*(const uint64_t*)(p+CH_SR_START);
    uint32_t io=*(const uint32_t*)(p+CH_IMAGES_OFF),ic=*(const uint32_t*)(p+CH_IMAGES_CNT);
    const ImgInfo*ii=(const ImgInfo*)(p+io);
    // find libsystem_platform
    int idx=-1;
    for(uint32_t i=0;i<ic;i++) if(strstr((const char*)(p+ii[i].pathFileOffset),"libsystem_platform.dylib")) { idx=(int)i; break; }
    const uint8_t*mh=(const uint8_t*)(ii[idx].address+slide);
    uint32_t ncmds=*(const uint32_t*)(mh+16),sz=*(const uint32_t*)(mh+20);
    const uint8_t*lc=mh+32;uint32_t eo=0,es=0;uint64_t lv=0,lf=0;
    for(uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);
        if(cs<8||off+cs>sz)break;
        if(cmd==0x19){const char*sg=(const char*)(lc+off+8);if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
        else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
        else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}
        off+=cs;}
    const uint8_t*trie=(const uint8_t*)(lv+slide)+(int64_t)eo-(int64_t)lf;
    const uint8_t*tend=trie+es; uint64_t base=ii[idx].address+slide;
    printf("libsystem_platform idx=%d base=0x%llx trie=%p size=0x%x\n",idx,(unsigned long long)base,trie,es);

    const char *names[] = {"__platform_strcmp","__platform_strncmp","__platform_memmove","__platform_bzero",NULL};
    for (int i=0;names[i];i++) {
        char pfx[256]; pfx[0]=0; uint64_t r1=0,r2=0; int found=0;
        collect(trie,tend,0,pfx,0,base,names[i],&r1,&r2,&found);
        if (!found) { printf("%-22s (not a SR leaf)\n",names[i]); continue; }
        const char *plain = names[i]+2; // strip __
        void *ds = dlsym(RTLD_DEFAULT, plain);
        printf("%-22s r1=0x%llx r2=0x%llx dlsym=%p\n",names[i],
               (unsigned long long)r1,(unsigned long long)r2,ds);
        printf("   CALL r1: ");
        if (!strcmp(names[i],"__platform_strcmp")) { int(*f)(const char*,const char*)=(int(*)(const char*,const char*))r1; printf("strcmp('abc','abd')=%d (want -1)\n",f("abc","abd")); }
        else if (!strcmp(names[i],"__platform_strncmp")) { int(*f)(const char*,const char*,unsigned long)=(int(*)(const char*,const char*,unsigned long))r1; printf("strncmp('abcx','abcy',3)=%d (want 0)\n",f("abcx","abcy",3)); }
        else if (!strcmp(names[i],"__platform_memmove")) { char a[16],b[16]; memset(a,0,16); strcpy(b,"hi"); void*(*f)(void*,const void*,size_t)=(void*(*)(void*,const void*,size_t))r1; f(a,b,3); printf("memmove->'%s' (want 'hi')\n",a); }
        else if (!strcmp(names[i],"__platform_bzero")) { char a[8]; memset(a,0x7f,8); void(*f)(void*,size_t)=(void(*)(void*,size_t))r1; f(a,4); printf("bzero-> %02x %02x %02x %02x (want 00)\n",(unsigned char)a[0],(unsigned char)a[1],(unsigned char)a[2],(unsigned char)a[3]); }
        fflush(stdout);
    }
    return 0;
}
