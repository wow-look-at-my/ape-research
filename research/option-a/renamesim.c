// renamesim.c: simulate Apple renaming/removing the names we know.
// The resolver iterates images and matches by EXPORTS ONLY -- it never reads the
// path -- so we prove independence by filtering out every image whose path
// contains "libSystem", "libSystem.B", or "libdyld", yet still resolving.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <dlfcn.h>

extern int cacheInit(void);
extern uint64_t g_cacheBase;
extern int64_t g_slide;
extern const uint8_t *g_cache;
extern uint32_t g_nImgs;

#define CH_IMAGES_OFF 0x1C0
typedef struct { uint64_t address, modTime, inode; uint32_t pathFileOffset, pad; } ImgInfo;

static uint64_t uleb(const uint8_t **p, const uint8_t *end, int *ok) {
    uint64_t r=0;int s=0;while(*p<end){uint8_t b=*(*p)++;r|=(uint64_t)(b&0x7f)<<s;if(!(b&0x80))return r;s+=7;if(s>63)break;}*ok=0;return 0;
}

// Resolve `plain`, but pretend every image whose path contains `hide` is gone.
static uint64_t resolveHide(const char *plain, const char *hide, char *who, size_t wsz) {
    char name[256]; int k=0; name[k++]='_';
    for (int i=0; plain[i] && k<255; i++) name[k++]=plain[i]; name[k]=0;
    uint32_t io = *(const uint32_t *)(g_cache + CH_IMAGES_OFF);
    const ImgInfo *ii = (const ImgInfo *)(g_cache + io);
    for (uint32_t i = 0; i < g_nImgs; i++) {
        const char *path = (const char *)(g_cache + ii[i].pathFileOffset);
        if (hide && *hide && strstr(path, hide)) continue;   // pretend it's renamed/gone
        const uint8_t *mh = (const uint8_t *)(ii[i].address + g_slide);
        if (*(const uint32_t *)mh != 0xfeedfacf) continue;
        uint32_t ncmds = *(const uint32_t *)(mh+16), sz = *(const uint32_t *)(mh+20);
        if (ncmds > 8192 || sz > 0x200000) continue;
        const uint8_t *lc = mh+32; uint32_t eo=0, es=0; uint64_t lv=0, lf=0;
        for (uint32_t j=0,off=0;j<ncmds&&off+8<=sz;j++){
            uint32_t cmd=*(const uint32_t*)(lc+off),cs=*(const uint32_t*)(lc+off+4);
            if(cs<8||off+cs>sz)break;
            if(cmd==0x19){const char*sg=(const char*)(lc+off+8);
                if(!strcmp(sg,"__LINKEDIT")){lv=*(const uint64_t*)(lc+off+24);lf=*(const uint64_t*)(lc+off+40);}}
            else if(cmd==0x80000033){eo=*(const uint32_t*)(lc+off+8);es=*(const uint32_t*)(lc+off+12);}
            else if(cmd==0x80000022&&!es){eo=*(const uint32_t*)(lc+off+40);es=*(const uint32_t*)(lc+off+44);}
            off+=cs;
        }
        if(!es||!lv)continue;
        const uint8_t *trie=(const uint8_t*)(lv+g_slide)+(int64_t)eo-(int64_t)lf;
        const uint8_t *end=trie+es,*node=trie;const char*s=name;
        for(int d=0;d<128;d++){
            int ok=1;const uint8_t*q=node;uint64_t ts=uleb(&q,end,&ok);
            if(!ok||q>end||ts>(uint64_t)(end-q))break;
            const uint8_t*term=q;q+=ts;
            if(*s==0&&ts){
                const uint8_t*t=term;uint64_t fl=uleb(&t,term+ts,&ok);
                if(fl&0x10){uint64_t u1=uleb(&t,term+ts,&ok);
                    if(who){int z=0;while(path[z]&&(size_t)z+1<wsz){who[z]=path[z];z++;}who[z]=0;}
                    return ii[i].address+g_slide+u1;}
                if(fl&0x08){uint64_t ord=uleb(&t,term+ts,&ok);(void)ord;break;}
                if(!(fl&0x02)){uint64_t a=uleb(&t,term+ts,&ok);
                    if(who){int z=0;while(path[z]&&(size_t)z+1<wsz){who[z]=path[z];z++;}who[z]=0;}
                    return ii[i].address+g_slide+a;}
                break;
            }
            uint64_t cc=uleb(&q,end,&ok);const uint8_t*fn=0;
            for(uint64_t x=0;x<cc;x++){const uint8_t*e=q;while(e<end&&*e)e++;size_t el=(size_t)(e-q);const uint8_t*ed=e+1;uint64_t co=uleb(&ed,end,&ok);
                if(!strncmp((const char*)q,s,el)){fn=trie+co;s+=el;break;}q=ed;}
            if(!fn)break;node=fn;
        }
    }
    return 0;
}

int main(void) {
    cacheInit();
    const char *syms[] = {"pthread_create","getentropy","mmap","dlopen","sem_open","sysctl","write","getpid",NULL};
    const char *hides[] = {"", "libSystem.B", "libSystem", "libdyld", NULL};
    for (int h = 0; hides[h]; h++) {
        int got=0, tot=0;
        for (int i=0; syms[i]; i++) {
            char who[192]; who[0]=0;
            uint64_t a = resolveHide(syms[i], hides[h], who, sizeof who);
            if (a) got++;
            tot++;
        }
        printf("hide='%-12s': resolved %d/%d\n", *hides[h]?hides[h]:"(nothing)", got, tot);
    }
    // Also: hide EVERY image -- nothing resolves, cleanly.
    {
        int got=0; char who[192];
        for (int i=0; syms[i]; i++) if (resolveHide(syms[i], "lib", who, sizeof who)) got++;
        printf("hide='lib' (all system libs): resolved %d/8 (expect 0)\n", got);
    }
    return 0;
}
