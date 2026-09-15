// vec.c: dump the "apple vector" (LC_MAIN 4th arg = x3) and try to find the
// shared cache base without syscall 294.
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;

static inline long sys3(long n, long a, long b, long c) {
    register long x16 __asm__("x16") = n;
    register long x0 __asm__("x0") = a, x1 __asm__("x1") = b, x2 __asm__("x2") = c;
    __asm__ volatile("svc #0x80" : "+r"(x0) : "r"(x16), "r"(x1), "r"(x2) : "memory", "cc");
    return x0;
}
static void outs(const char *s){ long n=0; while(s[n])n++; sys3(0x2000000|4,1,(long)s,n); }
static void outhex(u64 v){ char b[19]; b[0]='0';b[1]='x';
  for(int i=0;i<16;i++){int d=(v>>((15-i)*4))&0xf; b[2+i]=d<10?'0'+d:'a'+d-10;} b[18]=' '; sys3(0x2000000|4,1,(long)b,19); }
static void outdec(long v){ if(v<0){outs("-");v=-v;} char b[24];int i=23;b[i]=' '; if(!v)b[--i]='0';
  while(v){b[--i]='0'+v%10;v/=10;} sys3(0x2000000|4,1,(long)(b+i),24-i); }

int main(int argc, char **argv, char **envp) {
    register u64 *x3 __asm__("x3");
    u64 *vec = x3;
    outs("apple vector @ "); outhex((u64)vec); outs("\n");
    for (int i = 0; i < 12; i++) { outs("  vec["); outdec(i); outs("] = "); outhex(vec[i]); outs("\n"); }
    // The vector (per established facts) is 12 pointers then string data.
    // Try treating vec[0..] as char*.
    for (int i = 0; i < 12; i++) {
        char *p = (char *)vec[i];
        if (!p) continue;
        // plausible string?
        int printable = 1;
        for (int k = 0; k < 8; k++) { char c = p[k]; if (c && (c < 32 || c > 126)) { printable = 0; break; } if (!c) break; }
        if (printable && p[0]) { outs("  str["); outdec(i); outs("] = '"); sys3(0x2000000|4,1,(long)p,40); outs("'\n"); }
    }
    sys3(0x2000000|1, 0, 0, 0);
    return 0;
}
