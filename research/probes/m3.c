typedef long i64; typedef unsigned long u64;
static void w(const char*s,u64 n){register i64 x0 __asm__("x0")=2;register i64 x1 __asm__("x1")=(i64)s;register i64 x2 __asm__("x2")=(i64)n;register i64 x16 __asm__("x16")=0x2000004;__asm__ volatile("svc #0x80":"+r"(x0):"r"(x1),"r"(x2),"r"(x16):"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
static void ws(const char*s){u64 n=0;while(s[n])n++;w(s,n);}
static void wd(i64 v){char b[2];b[1]='\n';b[0]=(char)('0'+(v%10));w(b,2);}
int main(int argc, char **argv, char **envp) {
	ws("LC_MAIN entry: argc="); wd(argc);
	for (int i=0;i<argc && i<6;i++){ ws("  argv["); wd(i); ws("] = "); ws(argv[i]); ws("\n"); }
	int n=0; while(envp[n]) n++;
	ws("envc="); wd(n);
	return 0;
}
