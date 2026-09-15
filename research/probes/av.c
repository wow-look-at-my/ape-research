typedef unsigned long long u64;
static void w(const char*s,u64 n){register long x0 __asm__("x0")=2;register long x1 __asm__("x1")=(long)s;register long x2 __asm__("x2")=(long)n;register long x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wh(u64 v){char b[19];b[0]='0';b[1]='x';for(int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}b[18]='\n';w(b,19);}
int cmain(int argc, char **argv, char **envp, const char **apple);
__asm__(".globl _main\n_main:\n\tbl _cmain\n\tmovz x16,#0x200,lsl #16\n\tadd x16,x16,#1\n\tmov x0,#0\n\tsvc #0x80\n");
int cmain(int argc, char **argv, char **envp, const char **apple){
  (void)argc;(void)argv;(void)envp;
  ws("apple vector (4th arg to main):\n");
  for(int i=0;i<16;i++){ ws("  apple["); char d[3]; d[0]=(char)('0'+i/10); d[1]=(char)('0'+i%10); d[2]=0; ws(i<10?d+1:d); ws("] = "); wh((u64)apple[i]); }
  return 0;
}
