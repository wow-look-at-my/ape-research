/* The simpler route: mach trap task_info(TASK_DYLD_INFO=17) gives the loaded
   image list directly. No shared-cache header parsing needed at all. */
typedef unsigned long long u64; typedef long i64; typedef unsigned int u32; typedef unsigned char u8;
static i64 mtrap(i64 n,i64 a,i64 b,i64 c,i64 d){
  register i64 x0 __asm__("x0")=a;register i64 x1 __asm__("x1")=b;
  register i64 x2 __asm__("x2")=c;register i64 x3 __asm__("x3")=d;
  register i64 x16 __asm__("x16")=0x1000000|n;
  __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x16):
    "x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");
  return x0;}
static i64 bsd(i64 n,i64 a,i64 b,i64 c){return 0;}
static void ws(const char*s){u64 n=0;while(s[n])n++;
  {register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;
   __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';ws(b);}
static u32 r32(const u8*p){u32 v=0;for(int i=0;i<4;i++)v|=(u32)p[i]<<(8*i);return v;}
static u64 r64(const u8*p){u64 v=0;for(int i=0;i<8;i++)v|=(u64)p[i]<<(8*i);return v;}

/* mach trap numbers: 27 = task_self_trap, 44 = task_info */
#define TASK_SELF 27
#define TASK_INFO 44
#define TASK_DYLD_INFO 17
#define TASK_DYLD_INFO_COUNT 3

struct tdi { u64 addr, size, fmt; };

void cmain(i64 argc){(void)argc;
  ws("mach-trap route to the loaded image list\n");
  i64 self=mtrap(TASK_SELF,0,0,0,0);
  ws("  task_self_trap = "); wh((u64)self);
  if(self<=0){ws("  (no task port; cannot use task_info)\n");return;}
  struct tdi t; t.addr=t.size=t.fmt=0;
  i64 kr=mtrap(TASK_INFO,self,TASK_DYLD_INFO,(i64)&t,TASK_DYLD_INFO_COUNT);
  ws("  task_info kr = "); wh((u64)kr);
  ws("  all_image_info_addr = "); wh(t.addr);
  ws("  size = "); wh(t.size);
  if(!t.addr){ws("  (no dyld info)\n");return;}
  u8*p=(u8*)t.addr;
  ws("  dyld_all_image_infos.version = "); wh(r32(p));
  u32 cnt=r32(p+4);
  ws("  infoArrayCount = "); wh(cnt);
  u64 arr=r64(p+8);
  ws("  infoArray = "); wh(arr);
  if(!arr||cnt>10000){ws("  (implausible)\n");return;}
  int shown=0;
  for(u32 i=0;i<cnt;i++){
    u8*e=(u8*)(arr+(u64)i*24);   /* imageLoadAddress(8), imageFilePath(8), modDate(8) */
    u64 addr=r64(e), path=r64(e+8);
    if(!addr||!path)continue;
    const char*nm=(const char*)path;
    int is_ls=0;
    {const char*a=nm;const char*b="/usr/lib/libSystem.B.dylib";while(*a&&*a==*b){a++;b++;}if(!*a&&!*b)is_ls=1;}
    if(is_ls||shown<3){
      ws("    [");wh(i);ws("] ");wh(addr);ws("  ");
      for(int k=0;k<40&&nm[k];k++){char c=nm[k];if(c=='\n')break;
        {register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)&nm[k];register i64 x2 __asm__("x2")=1;register i64 x16 __asm__("x16")=0x2000004;
         __asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}}
      ws("\n");shown++;
    }
  }
  ws("  total images = "); wh(cnt);
}
__asm__(".globl _main\n_main:\n\tldr x0,[sp]\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
