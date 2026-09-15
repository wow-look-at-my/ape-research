// Is MAP_FIXED@0x100000000 failing because the address is occupied, or because
// it is not allowed at all? Test a range of addresses.
typedef unsigned long long u64; typedef long i64;
static u64 lastflags;
static i64 raw6f(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f,u64*fl){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n; u64 nz;
  __asm__ volatile("svc #0x80\n\tmrs %1, nzcv":"+r"(x0),"=r"(nz):
    "r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  *fl=nz; return x0;}
static void ws(const char*s){u64 n=0;while(s[n])n++;raw6f(0x2000004,2,(i64)s,(i64)n,0,0,0,&lastflags);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';raw6f(0x2000004,2,(i64)b,19,0,0,0,&lastflags);}
#define ANON 0x1002
#define FIXED 0x10
static void trymap(u64 addr,const char*label){
	u64 fl; i64 r=raw6f(0x2000000|197,addr,0x4000,3,ANON|FIXED,-1,0,&fl);
	ws(label); ws(" -> "); wh((u64)r);
	if(fl&0x20000000ULL){ ws("   FAIL errno="); wh((u64)r); }
	else if((u64)r==addr) ws("   OK exact\n");
	else { ws("   returned different addr "); wh((u64)r); }
}
void cmain(i64 argc){ (void)argc;
	ws("MAP_FIXED at candidate payload link addresses:\n");
	trymap(0x100000000ULL,"0x100000000  (cosmo amd64 base)  ");
	trymap(0x40000000000ULL,"0x40000000000 (cosmo arm64 base)");
	trymap(0x200000000ULL,"0x200000000  ");
	trymap(0x180000000ULL,"0x180000000  ");
	trymap(0x1000000000ULL,"0x1000000000 ");
	trymap(0x800000000ULL,"0x800000000  ");
	ws("\nIs 0x100000000 occupied? Check by mapping there WITHOUT MAP_FIXED and\nasking the kernel what it gave us, plus reading vm_region is a libSystem call.\n");
	ws("Instead: MAP_FIXED without MAP_ANON, no fd -> expect EBADF if addr is legal.\n");
	u64 fl; i64 r=raw6f(0x2000000|197,0x100000000ULL,0x4000,3,FIXED,-1,0,&fl);
	ws("  fixed,no-anon -> "); wh((u64)r); ws("  errno "); wh((u64)r); ws("\n");
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
