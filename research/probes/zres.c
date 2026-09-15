// resolver.c rebuilt with zero imports: no libc headers, no compiler-inserted
// strlen, no stack protector. Everything is our own code.
typedef unsigned char u8; typedef unsigned int u32; typedef unsigned long long u64; typedef long i64;
#define MH_MAGIC_64 0xfeedfacf
#define LC_SEGMENT_64 0x19
#define LC_SYMTAB 0x2
#define PAGE 0x4000
static i64 svc6(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;register i64 x2 __asm__("x2")=c;
  register i64 x3 __asm__("x3")=d;register i64 x4 __asm__("x4")=e;register i64 x5 __asm__("x5")=f;
  register i64 x16 __asm__("x16")=n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x16):
    "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0;}
static u32 r32(const u8*p){u32 v=0;for(int i=0;i<4;i++)v|=(u32)p[i]<<(8*i);return v;}
static u64 r64(const u8*p){u64 v=0;for(int i=0;i<8;i++)v|=(u64)p[i]<<(8*i);return v;}
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static u64 slen(const char*s){u64 n=0;while(s[n])n++;return n;}
static void ws(const char*s){svc6(0x2000004,2,(i64)s,(i64)slen(s),0,0,0);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';ws(b);}
static void wu(u64 v){char b[21];int i=20;b[i--]='\n';if(!v)b[i--]='0';while(v){b[i--]=(char)('0'+v%10);v/=10;}ws(b+i+1);}

static u64 shared_cache(void){u64 b=0;if(svc6(0x2000000|294,(i64)&b,0,0,0,0,0)!=0)return 0;return b;}

static const char*dylib_name(u64 addr){
  const u8*h=(const u8*)addr;
  if(r32(h)!=MH_MAGIC_64||r32(h+12)!=6)return 0;
  u32 ncmds=r32(h+16),sz=r32(h+20);
  if(!ncmds||ncmds>4096||!sz||sz>0x200000)return 0;
  const u8*cmd=h+32;
  for(u32 i=0;i<ncmds;i++){u32 c=r32(cmd),cs=r32(cmd+4);
    if(cs<8)return 0;
    if(c==0x0d){u32 no=r32(cmd+8);if(no>=cs)return 0;return(const char*)(cmd+no);}
    cmd+=cs;}
  return 0;}

static u64 image_symbol(u64 img,const char*name){
  const u8*h=(const u8*)img;
  u32 ncmds=r32(h+16);
  if(ncmds>4096)return 0;
  const u8*cmd=h+32;
  u64 textvm=0,leditvm=0,leditfo=0;
  u32 symoff=0,nsyms=0,stroff=0,strsize=0;
  for(u32 i=0;i<ncmds;i++){
    u32 c=r32(cmd),cs=r32(cmd+4);
    if(cs<8)return 0;
    if(c==LC_SEGMENT_64){
      const char*seg=(const char*)(cmd+8);
      if(seq(seg,"__TEXT"))textvm=r64(cmd+24);
      if(seq(seg,"__LINKEDIT")){leditvm=r64(cmd+24);leditfo=r64(cmd+40);}
    } else if(c==LC_SYMTAB){
      symoff=r32(cmd+8);nsyms=r32(cmd+12);stroff=r32(cmd+16);strsize=r32(cmd+20);
    }
    cmd+=cs;}
  if(!nsyms||!leditvm||!textvm)return 0;
  i64 slide=(i64)img-(i64)textvm;
  i64 delta=(i64)(leditvm+slide)-(i64)leditfo;
  const u8*st=(const u8*)((i64)stroff+delta);
  const u8*sm=(const u8*)((i64)symoff+delta);
  for(u32 i=0;i<nsyms;i++){
    const u8*e=sm+(u64)i*16;
    u32 strx=r32(e);
    if(strx>=strsize)continue;
    if(seq((const char*)(st+strx),name))return r64(e+8)+(u64)slide;
  }
  return 0;}

/* Bound the scan by the cache's own mapping table: mappingCount@0x14. */
static u64 find_symbol(const char*probe,const char*name){
  u64 cache=shared_cache();
  if(!cache)return 0;
  u64 limit=0x80000000ULL;   // 2 GB: the arm64e cache mapping is ~1.7 GB
  u64 found=0,nimg=0;
  for(u64 off=0;off<limit;off+=PAGE){
    u64 addr=cache+off;
    if(!dylib_name(addr))continue;
    nimg++;
    if(probe&&!image_symbol(addr,probe))continue;
    u64 p=image_symbol(addr,name);
    if(p&&!found){found=p;}
  }
  ws("     (scanned ");wu(limit);ws(" bytes, ");wu(nimg);ws(" dylibs)\n");
  return found;}

void cmain(i64 argc){(void)argc;
  ws("zero-import resolution (no libc at all)\n");
  u64 c=shared_cache(); ws("  cache = ");wh(c);
  const char*want[]={"getpid","mmap","mprotect","pread","getentropy","pthread_create","sysctl","sysctlbyname","exit","dlsym"};
  for(unsigned i=0;i<sizeof(want)/sizeof(want[0]);i++){
    ws("  ");ws(want[i]);
    u64 p=find_symbol("getpid",want[i]);
    if(!p)p=find_symbol("mmap",want[i]);
    ws(" -> ");wh(p);
  }
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
