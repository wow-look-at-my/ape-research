// Corrected: distinguish real addresses from negative errno. On Darwin the errno
// is returned in x0 with the carry flag set; from C the asm gives us the raw x0,
// so a small positive value is an error code, not a pointer.
typedef unsigned long long u64; typedef long i64; typedef unsigned char u8;
static u64 lastflags;
static i64 raw6f(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f,u64*fl){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n; u64 nz;
  __asm__ volatile("svc #0x80\n\tmrs %1, nzcv":"+r"(x0),"=r"(nz):
    "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  *fl=nz; return x0;}
static i64 raw6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){return raw6f(n,a,b,c,d,e,f,&lastflags);}
static void ws(const char*s){u64 n=0;while(s[n])n++;raw6(0x2000004,2,(i64)s,(i64)n,0,0,0);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';raw6(0x2000004,2,(i64)b,19,0,0,0);}
/* Show result plus how to tell success: carry set == error */
static void show(const char*label,u64*fl){
	i64 r=raw6(0x1,0,0,0,0,0,0); /* dummy no-op to keep signature simple */
	(void)r;(void)label;(void)fl;
}
#define ANON 0x1002
#define FIXED 0x10
static const char* cls(i64 r,u64 fl){
	if(fl&0x20000000ULL) return "ERROR (carry set)";
	return (r>0x100000000ULL)?"OK (real pointer)":"SUSPECT";
}
void cmain(i64 argc){ (void)argc;
	i64 r; u64 fl;
	ws("errno convention: carry (C, bit 29) set == failure\n\n");

	r=raw6f(0x2000000|197,0,0x4000,7,ANON,-1,0,&fl);
	ws("1. mmap RWX anon            -> "); wh((u64)r); ws("   "); ws(cls(r,fl)); ws("\n");

	r=raw6f(0x2000000|197,0,0x4000,3,ANON,-1,0,&fl);
	ws("2. mmap RW anon (control)   -> "); wh((u64)r); ws("   "); ws(cls(r,fl)); ws("\n");

	i64 rw = (fl&0x20000000ULL)?0:r;
	if(rw){
		u64 fl2;
		r=raw6f(0x2000000|74,rw,0x4000,5,0,0,0,&fl2);
		ws("3. mprotect that page RX    -> "); wh((u64)r); ws("   "); ws(cls(r,fl2)); ws("\n");
		r=raw6f(0x2000000|74,rw,0x4000,7,0,0,0,&fl2);
		ws("4. mprotect same page RWX   -> "); wh((u64)r); ws("   "); ws(cls(r,fl2)); ws("\n");
	}
	r=raw6f(0x2000000|197,0x100000000ULL,0x8000,3,ANON|FIXED,-1,0,&fl);
	ws("5. mmap FIXED @0x100000000  -> "); wh((u64)r); ws("   "); ws(cls(r,fl));
	ws(((fl&0x20000000ULL)==0 && r==(i64)0x100000000ULL)?"  EXACT\n":"\n");
	ws("   (0xc=ENOMEM, 0xd=EACCES, 0x1=EPERM are error codes, not addresses)\n");
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
