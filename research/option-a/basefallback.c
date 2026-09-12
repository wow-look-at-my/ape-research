// basefallback.c: can we find the shared cache base WITHOUT syscall 294?
// Strategy: scan the initial stack region for either
//   (a) the literal "dyld_v1" magic, or
//   (b) a pointer into the plausible cache window, validated by reading its header.
// Uses raw write/exit only; no libSystem calls.
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;

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

extern u64 saved_x3;
extern u64 saved_x0;

static int hdrValid(u64 a) {
    const u8 *p = (const u8 *)a;
    // magic "dyld_v1" and plausible imagesOffset/count
    if (p[0]!='d'||p[1]!='y'||p[2]!='l'||p[3]!='d'||p[4]!='_'||p[5]!='v'||p[6]!='1') return 0;
    u32 io = *(const u32 *)(p + 0x1C0);
    u32 ic = *(const u32 *)(p + 0x1C4);
    if (io < 0x200 || io > 0x10000) return 0;
    if (ic == 0 || ic > 100000) return 0;
    return 1;
}

int main(void) {
    // Use our own stack pointer as the scan origin.
    u64 sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    outs("sp="); outhex(sp); outs("\n");

    u64 found = 0;
    // The initial stack block (argv/envp/apple strings) lives ABOVE sp toward
    // the stack top, so scan upward. Stay within a modest, certainly-mapped
    // window and stop at the first hit.
    u64 hi = sp + 4u*1024*1024;
    for (u64 a = sp & ~7UL; a < hi; a += 8) {
        u64 v = *(const u64 *)a;
        if (v >= 0x180000000UL && v < 0x1A0000000UL && (v & 0x3FFF) == 0) {
            if (hdrValid(v)) { found = v; outs("pointer->header at stack+"); outdec((long)(a-sp)); outs(" -> "); outhex(v); outs("\n"); break; }
        }
    }
    if (!found) {
        for (u64 a = sp & ~7UL; a < hi; a += 8) {
            const u8 *q = (const u8 *)a;
            if (q[0]=='d'&&q[1]=='y'&&q[2]=='l'&&q[3]=='d'&&q[4]=='_'&&q[5]=='v'&&q[6]=='1') {
                u64 base = (u64)(a & ~0x3FFFULL);
                if (hdrValid(base)) { found = base; outs("magic at stack+"); outdec((long)(a-sp)); outs(" -> base "); outhex(base); outs("\n"); }
                else { outs("magic (unaligned) at stack+"); outdec((long)(a-sp)); outs("\n"); }
                break;
            }
        }
    }
    outs(found ? "FALLBACK SUCCEEDED\n" : "fallback found nothing\n");

    // Cross-check with syscall 294.
    u64 b294 = 0;
    long r = sys3(0x2000000|294, (long)&b294, 0, 0);
    outs("syscall 294 rc="); outdec(r); outs(" base="); outhex(b294); outs("\n");
    if (found && found == b294) outs("fallback == 294 result\n");
    sys3(0x2000000|1, 0, 0, 0);
    return 0;
}
