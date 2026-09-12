// Stress the loader mechanic: large mappings, multiple segments, MAP_FIXED at a
// fixed address (the payload's link address), and whether RWX is rejected.
typedef unsigned long long u64; typedef long i64; typedef unsigned char u8;
static i64 raw6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0;}
static void ws(const char*s){u64 n=0;while(s[n])n++;raw6(0x2000004,2,(i64)s,(i64)n,0,0,0);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';raw6(0x2000004,2,(i64)b,19,0,0,0);}
#define ANON 0x1002
#define FIXED 0x10
void cmain(i64 argc){ (void)argc;
	ws("1. RWX in one step (PROT_READ|WRITE|EXEC = 7), anon:\n");
	i64 p=raw6(0x2000000|197,0,0x4000,7,ANON,-1,0);
	ws("   mmap RWX -> "); wh((u64)p); ws(p<0?"   REJECTED (expected on arm64)\n":"   ACCEPTED\n");

	ws("2. MAP_FIXED at the payload's real link address 0x100000000:\n");
	i64 fixed=raw6(0x2000000|197,0x100000000ULL,0x8000,3,ANON|FIXED,-1,0);
	ws("   mmap FIXED -> "); wh((u64)fixed);
	ws(fixed==(i64)0x100000000ULL?"   GOT EXACT ADDRESS (payload can map at its link addr)\n":"   did not land exactly\n");

	ws("3. large mapping (64 MB), the size a real APE payload needs:\n");
	i64 big=raw6(0x2000000|197,0,64ULL<<20,3,ANON,-1,0);
	ws("   mmap 64MB -> "); wh((u64)big); ws(big>0?"   OK\n":"   FAIL\n");
	if(big>0){
		i64 r=raw6(0x2000000|74,big,64ULL<<20,5,0,0,0);
		ws("   mprotect RX over 64MB -> "); wh((u64)r); ws(r==0?"   OK\n":"   FAIL\n");
	}
	ws("4. MAP_JIT (0x800) - the hardened-runtime escape hatch:\n");
	i64 j=raw6(0x2000000|197,0,0x4000,7,ANON|0x800,-1,0);
	ws("   mmap RWX|MAP_JIT -> "); wh((u64)j); ws(j<0?"   REJECTED\n":"   ACCEPTED\n");
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
