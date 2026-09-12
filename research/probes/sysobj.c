// sysobj.c - the loader's syscall layer, with NO libSystem imports.
// Syscall numbers taken from the SDK's <sys/syscall.h>, never guessed.
//
// This is the piece that buys ABI stability: these are kernel entry points
// whose numbers Apple has kept fixed for decades, unlike libc's exported C
// functions, whose set and behaviour churn between releases.
typedef long i64;
typedef unsigned long u64;
typedef unsigned char u8;
typedef unsigned int u32;

// BSD syscall class is 0x2000000.
#define BSD(n) (0x2000000 | (n))

// From <sys/syscall.h> on this machine.
enum {
	SYS_exit        = 1,
	SYS_fork        = 2,
	SYS_read        = 3,
	SYS_write       = 4,
	SYS_close       = 6,
	SYS_getuid      = 24,
	SYS_geteuid     = 25,
	SYS_getegid     = 43,
	SYS_getgid      = 47,
	SYS_kill        = 37,
	SYS_pipe        = 42,
	SYS_sigaction   = 46,
	SYS_sigaltstack = 53,
	SYS_munmap      = 73,
	SYS_mprotect    = 74,
	SYS_pread       = 153,
	SYS_getrlimit   = 194,
	SYS_setrlimit   = 195,
	SYS_mmap        = 197,
	SYS_sysctl      = 202,
	SYS_sem_open    = 268,
	SYS_sem_close   = 269,
	SYS_sem_unlink  = 270,
	SYS_sem_wait    = 271,
	SYS_sem_trywait = 272,
	SYS_sem_post    = 273,
	SYS_issetugid   = 327,
	SYS_pselect     = 394,
	SYS_openat      = 463,
	SYS_getentropy  = 500,
	// 294 is shared_region_check_np, used by the resolver.
	SYS_shared_region_check_np = 294,
};

// Raw syscall. Darwin returns the error number in x0 with the carry flag set
// on failure, so callers that want -errno must use svc_errno below.
static i64 svc6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x3 __asm__("x3") = d;
	register i64 x4 __asm__("x4") = e;
	register i64 x5 __asm__("x5") = f;
	register i64 x16 __asm__("x16") = n;
	__asm__ volatile("svc #0x80" : "+r"(x0)
	                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x16)
	                 : "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17",
	                   "memory","cc");
	return x0;
}

// Translate Darwin's carry-flag convention into the -errno convention the
// cosmo payload's Syslib expects.
static i64 svc_errno(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
	register i64 x0 __asm__("x0") = a;
	register i64 x1 __asm__("x1") = b;
	register i64 x2 __asm__("x2") = c;
	register i64 x3 __asm__("x3") = d;
	register i64 x4 __asm__("x4") = e;
	register i64 x5 __asm__("x5") = f;
	register i64 x16 __asm__("x16") = n;
	u64 flags;
	__asm__ volatile("svc #0x80\n\tmrs %1, nzcv"
	                 : "+r"(x0), "=r"(flags)
	                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x16)
	                 : "x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17",
	                   "memory","cc");
	// C is bit 29 of NZCV.
	return (flags & (1ULL << 29)) ? -x0 : x0;
}

// Syslib-compatible wrappers. Each returns the syscall result, or -errno.
static i64 w_fork(void)                                      { return svc_errno(BSD(SYS_fork), 0,0,0,0,0,0); }
static i64 w_pipe(int p[2])                                  { return svc_errno(BSD(SYS_pipe), (i64)p,0,0,0,0,0); }
static i64 w_close(int fd)                                   { return svc_errno(BSD(SYS_close), fd,0,0,0,0,0); }
static i64 w_munmap(void *a, u64 n)                          { return svc_errno(BSD(SYS_munmap), (i64)a, (i64)n,0,0,0,0); }
static i64 w_mprotect(void *a, u64 n, int prot)              { return svc_errno(BSD(SYS_mprotect), (i64)a, (i64)n, prot,0,0,0); }
static i64 w_openat(int d, const char *p, int f, int m)      { return svc_errno(BSD(SYS_openat), d, (i64)p, f, m,0,0); }
static i64 w_write(int fd, const void *b, u64 n)             { return svc_errno(BSD(SYS_write), fd, (i64)b, (i64)n,0,0,0); }
static i64 w_read(int fd, void *b, u64 n)                    { return svc_errno(BSD(SYS_read), fd, (i64)b, (i64)n,0,0,0); }
static i64 w_getentropy(void *b, u64 n)                      { return svc_errno(BSD(SYS_getentropy), (i64)b, (i64)n,0,0,0,0); }
static i64 w_sigaction(int s, const void *a, void *o)        { return svc_errno(BSD(SYS_sigaction), s, (i64)a, (i64)o,0,0,0); }
static i64 w_sigaltstack(const void *s, void *o)             { return svc_errno(BSD(SYS_sigaltstack), (i64)s, (i64)o,0,0,0,0); }
static i64 w_sysctl(int *m, u32 n, void *o, u64 *ol, void *nw, u64 nl) {
	return svc_errno(BSD(SYS_sysctl), (i64)m, n, (i64)o, (i64)ol, (i64)nw, (i64)nl);
}
static i64 w_getrlimit(int r, void *l)                       { return svc_errno(BSD(SYS_getrlimit), r, (i64)l,0,0,0,0); }
static i64 w_setrlimit(int r, const void *l)                 { return svc_errno(BSD(SYS_setrlimit), r, (i64)l,0,0,0,0); }
static i64 w_kill(int pid, int sig)                          { return svc_errno(BSD(SYS_kill), pid, sig,0,0,0,0); }
static i64 w_sem_open(const char *n, int f, unsigned m, unsigned v) {
	return svc_errno(BSD(SYS_sem_open), (i64)n, f, m, v,0,0);
}
static i64 w_sem_unlink(const char *n)                       { return svc_errno(BSD(SYS_sem_unlink), (i64)n,0,0,0,0,0); }
static i64 w_sem_close(void *s)                              { return svc_errno(BSD(SYS_sem_close), (i64)s,0,0,0,0,0); }
static i64 w_sem_post(void *s)                               { return svc_errno(BSD(SYS_sem_post), (i64)s,0,0,0,0,0); }
static i64 w_sem_wait(void *s)                               { return svc_errno(BSD(SYS_sem_wait), (i64)s,0,0,0,0,0); }
static i64 w_sem_trywait(void *s)                            { return svc_errno(BSD(SYS_sem_trywait), (i64)s,0,0,0,0,0); }
static i64 w_pselect(int n, void *r, void *w, void *e, const void *t, const void *m) {
	return svc_errno(BSD(SYS_pselect), n, (i64)r, (i64)w, (i64)e, (i64)t, (i64)m);
}
// mmap through the syscall. Darwin's mmap returns the address directly and
// signals failure with the carry flag, so no MAP_FAILED sentinel is needed.
static i64 w_mmap(void *a, u64 n, int prot, int flags, int fd, i64 off) {
	return svc_errno(BSD(SYS_mmap), (i64)a, (i64)n, prot, flags, fd, off);
}
// exit is noreturn and needs no errno translation.
__attribute__((noreturn)) static void w_exit(int code) {
	svc6(BSD(SYS_exit), code, 0,0,0,0,0);
	__builtin_unreachable();
}
static u64 w_getuid(void)  { return (u64)svc6(BSD(SYS_getuid), 0,0,0,0,0,0); }
static u64 w_geteuid(void) { return (u64)svc6(BSD(SYS_geteuid), 0,0,0,0,0,0); }
static u64 w_getgid(void)  { return (u64)svc6(BSD(SYS_getgid), 0,0,0,0,0,0); }
static u64 w_getegid(void) { return (u64)svc6(BSD(SYS_getegid), 0,0,0,0,0,0); }
static u64 w_issetugid(void){ return (u64)svc6(BSD(SYS_issetugid), 0,0,0,0,0,0); }

// --- self-test ------------------------------------------------------------
static void ws(const char *s) { u64 n = 0; while (s[n]) n++; svc6(BSD(SYS_write), 2, (i64)s, (i64)n, 0,0,0); }
static void wh(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}
	b[18]='\n'; ws(b);
}
static void wd(i64 v) {
	char b[24]; int i = 22; b[i--] = '\n';
	int neg = v < 0; if (neg) v = -v;
	if (!v) b[i--] = '0';
	while (v) { b[i--] = (char)('0' + v % 10); v /= 10; }
	if (neg) b[i--] = '-';
	ws(b + i + 1);
}

void cmain(i64 argc) {
	(void)argc;
	ws("syscall layer, zero libSystem imports\n");
	ws("  getuid  = "); wd((i64)w_getuid());
	ws("  getgid  = "); wd((i64)w_getgid());
	ws("  issetugid = "); wd((i64)w_issetugid());

	i64 p[2];
	ws("  pipe    = "); wd(w_pipe((int *)p));
	ws("  write   = "); wd(w_write(2, "ok\n", 3));
	ws("  write EBADF = "); wd(w_write(999, "x", 1));

	i64 m = w_mmap(0, 0x4000, 3 /*RW*/, 0x1002 /*PRIVATE|ANON*/, -1, 0);
	ws("  mmap    = "); wh((u64)m);
	if (m > 0) {
		ws("  mprotect= "); wd(w_mprotect((void *)m, 0x4000, 5 /*RX*/));
		ws("  munmap  = "); wd(w_munmap((void *)m, 0x4000));
	}

	i64 e = w_getentropy((void *)p, 16);
	ws("  getentropy = "); wd(e);

	// exit is exercised last since it never returns.
	ws("  all syscall wrappers callable; exiting 0\n");
	w_exit(0);
}
__asm__(".globl _main\n"
        "_main:\n"
        "\tldr x0, [sp]\n"
        "\tbl _cmain\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
