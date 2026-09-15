// syslibdemo.c: build an entire APE-style Syslib table for arm64 by CONTENT
// DISCOVERY, with zero dylib load commands and zero undefined symbols.
// Proves Option A can supply every pointer the payload needs.
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8; typedef int64_t i64;

static inline long sys3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a, x1 __asm__("x1") = b, x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory", "cc");
    return x0;
}
static void out(const char *s, long n){ sys3(0x2000000|4,1,(long)s,n); }
static void outs(const char *s){ long n=0; while(s[n])n++; out(s,n); }
static void outhex(u64 v){ char b[19]; b[0]='0';b[1]='x';
  for(int i=0;i<16;i++){int d=(v>>((15-i)*4))&0xf; b[2+i]=d<10?'0'+d:'a'+d-10;} b[18]=' '; out(b,19); }
static void outdec(long v){ if(v<0){outs("-");v=-v;} char b[24];int i=23;b[i]=' ';
  if(!v)b[--i]='0'; while(v){b[--i]='0'+v%10;v/=10;} out(b+i,24-i); }

#define CH_SR_START   0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4
typedef struct { u64 address, modTime, inode; u32 pathFileOffset, pad; } ImgInfo;

static u64 g_cache;
static u64 g_slide;

static u64 uleb(const u8 **p, const u8 *end, int *ok) {
    u64 r=0;int sh=0;while(*p<end){u8 b=*(*p)++;r|=(u64)(b&0x7f)<<sh;if(!(b&0x80))return r;sh+=7;if(sh>63)break;}*ok=0;return 0;
}

// Trie walk returning kind + payload. kind: 0 none, 1 addr, 2 reexport, 3 stub/res, 4 abs, 5 tls
typedef struct { int kind; u64 a, b; u32 ordinal; char imp[128]; } Hit;

static Hit trieLookup(const u8 *trie, u64 size, const char *name) {
    Hit h; h.kind=0; h.a=h.b=0; h.ordinal=0; h.imp[0]=0;
    if (!trie || size < 4) return h;
    const u8 *end=trie+size,*node=trie;const char*s=name;
    for(int d=0;d<128;d++){
        int ok=1; const u8*q=node; u64 ts=uleb(&q,end,&ok);
        if(!ok||q>end||ts>(u64)(end-q)) return h;
        const u8*term=q; q+=ts;
        if(*s==0&&ts){
            const u8*t=term; u64 fl=uleb(&t,term+ts,&ok); if(!ok) return h;
            if(fl&0x08){ h.kind=2; h.ordinal=(u32)uleb(&t,term+ts,&ok);
                u64 n=0; while(t<term+ts&&*t&&n<127) h.imp[n++]=*t++; h.imp[n]=0; return h; }
            if(fl&0x10){ h.kind=3; h.a=uleb(&t,term+ts,&ok); h.b=uleb(&t,term+ts,&ok); return h; }
            if(fl&0x02){ h.kind=4; h.a=uleb(&t,term+ts,&ok); return h; }
            if(fl&0x04){ h.kind=5; h.a=uleb(&t,term+ts,&ok); return h; }
            h.kind=1; h.a=uleb(&t,term+ts,&ok); return h;
        }
        u64 cc=uleb(&q,end,&ok); if(!ok||cc>8192) return h;
        const u8*found=0;
        for(u64 i=0;i<cc;i++){
            const u8*e=q; while(e<end&&*e)e++; if(e>=end) return h;
            u64 el=(u64)(e-q); const u8*ed=e+1; u64 co=uleb(&ed,end,&ok); if(!ok) return h;
            int same=1; for(u64 k=0;k<el;k++) if(s[k]!=q[k]){same=0;break;}
            if(same){ found=trie+co; s+=el; break; }
            q=ed;
        }
        if(!found) return h; node=found;
    }
    return h;
}

// Per-image export info cache (computed on demand).
#define MAXIMG 4096
typedef struct { u64 base; u32 eo, es; u64 lv, lf; const u8 *trie; } ImgX;
static ImgX g_ix[MAXIMG];
static u32 g_n;

static void loadImage(u32 i) {
    const u8 *p=(const u8*)g_cache;
    u32 ioOff = *(const u32*)(p + CH_IMAGES_OFF);
    const ImgInfo*ii=(const ImgInfo*)(p+ioOff);
    g_ix[i].base=ii[i].address+g_slide;
    const u8*mh=(const u8*)(ii[i].address+g_slide);
    g_ix[i].eo=g_ix[i].es=0; g_ix[i].lv=g_ix[i].lf=0; g_ix[i].trie=0;
    if (mh[0]!=0xcf||mh[1]!=0xfa||mh[2]!=0xed||mh[3]!=0xfe) return;
    u32 ncmds=*(const u32*)(mh+16),sz=*(const u32*)(mh+20);
    if (ncmds>8192||sz>0x200000) return;
    const u8*lc=mh+32;
    for(u32 j=0,off=0;j<ncmds&&off+8<=sz;j++){
        u32 cmd=*(const u32*)(lc+off),cs=*(const u32*)(lc+off+4);
        if(cs<8||off+cs>sz)break;
        if(cmd==0x19){
            const char*sg=(const char*)(lc+off+8);
            if(sg[0]=='_'&&sg[1]=='_'&&sg[2]=='L'&&sg[3]=='I'&&sg[4]=='N'&&sg[5]=='K'&&sg[6]=='E'&&sg[7]=='D'&&sg[8]=='I'&&sg[9]=='T'&&sg[10]==0){
                g_ix[i].lv=*(const u64*)(lc+off+24); g_ix[i].lf=*(const u64*)(lc+off+40);
            }
        } else if(cmd==0x80000033){ g_ix[i].eo=*(const u32*)(lc+off+8); g_ix[i].es=*(const u32*)(lc+off+12); }
        else if(cmd==0x80000022&&!g_ix[i].es){ g_ix[i].eo=*(const u32*)(lc+off+40); g_ix[i].es=*(const u32*)(lc+off+44); }
        off+=cs;
    }
    if(g_ix[i].es&&g_ix[i].lv)
        g_ix[i].trie=(const u8*)(g_ix[i].lv+g_slide)+(i64)g_ix[i].eo-(i64)g_ix[i].lf;
}

// Resolve plain symbol name by content across every image.
static u64 resolve(const char *plain, char *who, u64 whosz) {
    char name[256]; int k=0; name[k++]='_';
    for(int i=0;plain[i]&&k<255;i++) name[k++]=plain[i]; name[k]=0;
    for(u32 i=0;i<g_n;i++){
        if(!g_ix[i].trie) continue;
        Hit h=trieLookup(g_ix[i].trie,g_ix[i].es,name);
        if(h.kind==1||h.kind==5) { if(who&&whosz){const char*pp=(const char*)((const u8*)g_cache+((const ImgInfo*)((const u8*)g_cache+*(const u32*)((const u8*)g_cache+CH_IMAGES_OFF)))[i].pathFileOffset); int z=0;while(pp[z]&&(u64)z+1<whosz){who[z]=pp[z];z++;}who[z]=0;} return g_ix[i].base+h.a; }
        if(h.kind==3) { if(who&&whosz){const char*pp=(const char*)((const u8*)g_cache+((const ImgInfo*)((const u8*)g_cache+*(const u32*)((const u8*)g_cache+CH_IMAGES_OFF)))[i].pathFileOffset); int z=0;while(pp[z]&&(u64)z+1<whosz){who[z]=pp[z];z++;}who[z]=0;} return g_ix[i].base+h.a; }
        if(h.kind==2){
            char nn[256]; int m=0;
            if(h.imp[0]){ nn[m++]='_'; for(int x=0;h.imp[x]&&m<255;x++) nn[m++]=h.imp[x]; }
            else for(int x=0;name[x]&&m<255;x++) nn[m++]=name[x];
            nn[m]=0;
            for(u32 j=0;j<g_n;j++){
                if(!g_ix[j].trie) continue;
                Hit h2=trieLookup(g_ix[j].trie,g_ix[j].es,nn);
                if(h2.kind==1||h2.kind==5) return g_ix[j].base+h2.a;
                if(h2.kind==3) return g_ix[j].base+h2.a;
            }
        }
    }
    return 0;
}

// The Syslib contract (order matches ape-m1.c / os_cosmo_arm64.go).
static const char *SYSLIB[] = {
 "fork","pipe","clock_gettime","nanosleep","mmap",
 "pthread_jit_write_protect_supported_np","pthread_jit_write_protect_np","sys_icache_invalidate",
 "pthread_create","pthread_exit","pthread_kill","pthread_sigmask","pthread_setname_np",
 "dispatch_semaphore_create","dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime",
 "pthread_self","dispatch_release","raise","pthread_join","pthread_yield_np",
 "pthread_attr_init","pthread_attr_destroy","pthread_attr_setstacksize","pthread_attr_setguardsize",
 "exit","close","munmap","openat","write","read","sigaction","pselect","mprotect",
 "sigaltstack","getentropy","sem_open","sem_unlink","sem_close","sem_post","sem_wait","sem_trywait",
 "getrlimit","setrlimit","dlopen","dlsym","dlclose","dlerror","pthread_cpu_number_np",
 "sysctl","sysctlbyname","sysctlnametomib"
};
#define NSYS ((int)(sizeof SYSLIB / sizeof SYSLIB[0]))
static u64 g_tab[NSYS];

int main(void) {
    u64 base=0;
    long rc=sys3(0x2000000|294,(long)&base,0,0);
    if(rc||!base){ outs("no cache\n"); sys3(0x2000000|1,1,0,0); }
    g_cache=base;
    g_slide=base-*(const u64*)((const u8*)base+CH_SR_START);
    g_n=*(const u32*)((const u8*)base+CH_IMAGES_CNT);
    if(g_n>MAXIMG) g_n=MAXIMG;
    for(u32 i=0;i<g_n;i++) loadImage(i);

    outs("building Syslib from content: "); outdec(g_n); outs(" cache images\n");
    int miss=0;
    for(int i=0;i<NSYS;i++){
        char who[192]; who[0]=0;
        u64 a=resolve(SYSLIB[i],who,sizeof who);
        g_tab[i]=a;
        if(!a) miss++;
        outs("  ["); outdec(i); outs("] "); outs(SYSLIB[i]); outs(a?" -> ":" -> MISSING ");
        if(a){ outhex(a); outs(" "); outs(who); }
        outs("\n");
    }
    outs("resolved "); outdec(NSYS-miss); outs("/"); outdec(NSYS); outs(" Syslib entries\n");

    // Prove a few are callable.
    if(g_tab[0]) { long (*f)(void)=(long(*)(void))g_tab[0]; long r=f(); outs("fork() via table = "); outdec(r); outs(" (0 = child)\n");
        if(r==0) sys3(0x2000000|1,0,0,0); }
    if(g_tab[26]) { void (*ex)(int)=(void(*)(int))g_tab[26]; outs("exit ptr = "); outhex(g_tab[26]); outs(" (not called)\n"); }
    if(g_tab[30]) { long (*wr)(int,const void*,size_t)=(long(*)(int,const void*,size_t))g_tab[30];
        wr(1,"write() via content-discovered table works\n",43); }
    if(g_tab[36]) { long (*ge)(void*,size_t)=(long(*)(void*,size_t))g_tab[36]; u8 buf[16]; long r=ge(buf,16);
        outs("getentropy via table rc="); outdec(r); outs(" bytes="); outhex(*(u64*)buf); outs("\n"); }
    sys3(0x2000000|1,0,0,0);
    return 0;
}
