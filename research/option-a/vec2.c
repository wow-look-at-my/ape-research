// vec2.c: with x3 captured by the asm shim, dump the apple vector and test
// whether it can yield the shared cache base without syscall 294.
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;
extern u64 saved_x3, saved_x0;

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

static u64 rd64(u64 *p){ return *p; }

int main(void) {
    outs("saved_x3 = "); outhex(saved_x3); outs("\n");
    outs("saved_x0 = "); outhex(saved_x0); outs("\n");
    u64 *vec = (u64 *)saved_x3;
    if (!vec) { outs("x3 NULL\n"); sys3(0x2000000|1,0,0,0); }
    for (int i = 0; i < 16; i++) { outs("  vec["); outdec(i); outs("] = "); outhex(vec[i]); outs("\n"); }
    // Treat as char* strings.
    for (int i = 0; i < 16; i++) {
        char *p = (char *)vec[i];
        if (!p) continue;
        int ok = 1;
        for (int k = 0; k < 12; k++) { char c = p[k]; if (c && (unsigned char)c < 32) { ok = 0; break; } if (!c) break; }
        if (ok && p[0]) { outs("  str["); outdec(i); outs("] = '"); out(p, 48); outs("'\n"); }
        else { outs("  [nonstring "); outdec(i); outs("] = "); outhex((u64)p); outs("\n"); }
    }
    // Scan the vector's memory region for the cache magic.
    char *region = (char *)saved_x3;
    for (long off = 0; off < 0x4000; off += 8) {
        if (region[off] == 'd' && region[off+1] == 'y' && region[off+2] == 'l' && region[off+3] == 'd'
            && region[off+4] == '_' && region[off+5] == 'v' && region[off+6] == '1') {
            outs("FOUND cache magic at vec+"); outdec(off); outs("\n");
        }
    }
    sys3(0x2000000|1, 0, 0, 0);
    return 0;
}
