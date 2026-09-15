typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
static i64 svc(i64 n,i64 a){register i64 x0 __asm__("x0")=a;register i64 x16 __asm__("x16")=n;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");return x0;}
static i64 parse(const char*s){i64 v=0;if(s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2;for(;*s;s++){int d;if(*s>='0'&&*s<='9')d=*s-'0';else if(*s>='a'&&*s<='f')d=*s-'a'+10;else if(*s>='A'&&*s<='F')d=*s-'A'+10;else break;v=(v<<4)|d;}return v;}
void go(i64 argc, char **argv){
  if(argc<2){ws("no arg\n");return;}
  i64 n=parse(argv[1]);
  ws("svc x16=");wh((u64)n);
  i64 r=svc(n,0);
  ws("   ret=");wh((u64)r);
}
