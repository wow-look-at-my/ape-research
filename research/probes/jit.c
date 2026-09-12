// Can a ZERO-IMPORT arm64 binary create executable memory with raw syscalls?
// This is the loader's core mechanic: map segments, make them executable, jump.
typedef unsigned long long u64; typedef long i64; typedef unsigned char u8;
static i64 raw6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x0 __asm__("x0")=a; register i64 x1 __asm__("x1")=b;
  register i64 x2 __asm__("x2")=c; register i64 x3 __asm__("x3")=d;
  register i64 x4 __asm__("x4")=e; register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0;}
static void ws(const char*s){u64 n=0;while(s[n])n++;raw6(0x2000004,2,(i64)s,(i64)n,0,0,0);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';raw6(0x2000004,2,(i64)b,19,0,0,0);}
static void wd(i64 v){char b[24];int i=23;b[i--]='\n';if(!v)b[i--]='0';int neg=v<0;if(neg)v=-v;while(v){b[i--]=(char)('0'+v%10);v/=10;}if(neg)b[i--]='-';raw6(0x2000004,2,(i64)(b+i+1),(i64)(23-i),0,0,0);}

#define PROT_RW  3
#define PROT_RX  5
#define MAP_PRIVATE_ANON 0x1002
#define MAP_JIT  0x800

// int main-ish, LC_MAIN entry: x0=argc
void cmain(i64 argc){
	(void)argc;
	// 1. anonymous RW mapping
	i64 p = raw6(0x2000000|197, 0, 0x4000, PROT_RW, MAP_PRIVATE_ANON, -1, 0);
	ws("mmap RW anon  -> "); wh((u64)p); if(p<0){ws("FAIL\n");return;}

	// 2. emit a tiny function that returns 42
	//    mov w0, #42 ; ret
	u8 code[] = {0x40,0x05,0x80,0x52, 0xc0,0x03,0x5f,0xd6};
	for(int i=0;i<8;i++) ((u8*)p)[i]=code[i];

	// 3. make it executable
	i64 r = raw6(0x2000000|74, p, 0x4000, PROT_RX, 0, 0, 0);
	ws("mprotect RX   -> "); wh((u64)r);
	if(r!=0){ ws("  (mprotect failed)\n"); }
	// 4. flush icache: sys_icache_invalidate is a libSystem call. Raw option? test without.
	// 5. jump
	typedef int (*f_t)(void);
	f_t f=(f_t)p;
	int v=f();
	ws("called mapped code -> returned "); wd((i64)v);
	ws(v==42?"  JUMP OK (W^X permitted for RW->RX)\n":"  WRONG VALUE\n");
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
