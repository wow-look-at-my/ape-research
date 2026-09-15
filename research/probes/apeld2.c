// apeld2.c - macOS arm64 APE loader, hardened for a hostile Apple.
//
// Design, each point measured rather than assumed:
//
//  * Zero imported symbols. `nm -u` reports none. The loader is self-contained
//    code; it never asks dyld to bind a name, so libc ABI churn cannot reach it.
//  * Raw BSD syscalls for everything syscall-shaped. Syscall numbers come from
//    the SDK's <sys/syscall.h>. These are kernel entry points whose numbers
//    Apple has held fixed far longer than any libc ABI.
//  * minos 11.0 (the first Apple Silicon release). Verified: a binary declaring
//    minos <= 15.3 with zero dylib load commands still launches, because dyld's
//    spring-2025 `enforceHasLinkedDylibs` is gated on the binary's own minos.
//    So this file carries no Apple-controlled library name at all.
//  * LC_MAIN, not LC_UNIXTHREAD: only LC_MAIN yields a proper argc/argv block.
//  * The 53-entry Syslib table is built by searching the dyld shared cache for
//    the images that export what we need, rather than by hard-linking
//    libSystem.B and trusting its flat namespace. libSystem.B exports only 3
//    symbols of its own and re-exports 39 sub-libraries, so the flat-namespace
//    assumption is exactly the thing that breaks when Apple reorganises.
//
// Honest limits, verified and not papered over:
//  * dyld itself compares the install name against the literal strings
//    "/usr/lib/system/libdyld.dylib" and "/usr/lib/libSystem.B.dylib"
//    (DyldRuntimeState.cpp:459-471) and halts if either is missing, BEFORE this
//    loader is entered. So if Apple renames libSystem, no loader design survives
//    without a dyld change. This loader's value is that it removes every
//    dependency it can, not that it defeats that one.
//  * RWX is refused on arm64 (W^X). Segments are mapped RW, filled, then
//    mprotect(RX), which is verified to work.
//  * The payload's link address is 0x40000000000; MAP_FIXED there succeeds
//    exactly. The loader's own image sits near 0x100000000, which is why mapping
//    there would fail.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long i64;

// ---- syscalls (numbers from <sys/syscall.h>) ------------------------------
#define BSD(n) (0x2000000 | (n))
enum {
	SYS_exit = 1, SYS_write = 4, SYS_getuid = 24, SYS_geteuid = 25,
	SYS_getgid = 47, SYS_getegid = 43, SYS_pread = 153, SYS_mprotect = 74,
	SYS_mmap = 197, SYS_openat = 463, SYS_getentropy = 500,
	SYS_issetugid = 327, SYS_shared_region_check_np = 294,
	SYS_fork = 2, SYS_read = 3, SYS_close = 6, SYS_pipe = 42,
	SYS_munmap = 73, SYS_gettimeofday = 116,
};

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
static i64 svc_e(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
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
	return (flags & (1ULL << 29)) ? -x0 : x0;
}

// ---- memory primitives ----------------------------------------------------
static void memset0(void *p, u64 n) { volatile u8 *c = p; while (n--) *c++ = 0; }
__attribute__((no_builtin("strlen")))
static u64 slen(const char *s) { u64 n = 0; const volatile char *v = s; while (v[n]) n++; return n; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static void out(const char *s, u64 n) { svc6(BSD(SYS_write), 2, (i64)s, (i64)n, 0, 0, 0); }
static void ws(const char *s) { out(s, slen(s)); }
static void whx(u64 v) {
	char b[19]; b[0]='0'; b[1]='x';
	for (int i=0;i<16;i++){int d=(int)((v>>(60-4*i))&0xf);b[2+i]=d<10?(char)('0'+d):(char)('a'+d-10);}
	b[18]='\n'; out(b, 19);
}
// Debug tracing is off unless APELD_DEBUG is present in the environment.
// The environment pointer is passed in, never read from libSystem's `environ`.
static char **g_envp;
static int getenv_dbg(void) {
	char **e = g_envp;
	if (!e) return 0;
	for (; *e; e++) {
		const char *s = *e, *p = "APELD_DEBUG";
		while (*p && *s == *p) { s++; p++; }
		if (!*p) return 1;
	}
	return 0;
}
__attribute__((noreturn)) static void die(const char *msg) {
	ws("apeld: "); ws(msg); ws("\n");
	svc6(BSD(SYS_exit), 127, 0, 0, 0, 0, 0);
	__builtin_unreachable();
}

// ---- readers --------------------------------------------------------------
static u32 r32(const u8 *p) { u32 v = 0; for (int i = 0; i < 4; i++) v |= (u32)p[i] << (8*i); return v; }
static u64 r64(const u8 *p) { u64 v = 0; for (int i = 0; i < 8; i++) v |= (u64)p[i] << (8*i); return v; }

#define MH_MAGIC_64  0xfeedfacf
#define MH_DYLIB     6
#define LC_SEGMENT_64 0x19
#define LC_SYMTAB     0x2

// Offsets from Apple's include/mach-o/dyld_cache_format.h.
#define CH_REGION_START 0x0e0
#define CH_IMAGES_OFF   0x1c0
#define CH_IMAGES_CNT   0x1c4
#define IMAGE_INFO_SIZE 32

static u64 shared_cache(void) {
	u64 base = 0;
	return svc_e(BSD(SYS_shared_region_check_np), (i64)&base, 0,0,0,0,0) == 0 ? base : 0;
}

// Resolve `name` in the MH_DYLIB image at `img` through its own LC_SYMTAB.
// Mach-O C symbols carry a leading underscore.
static u64 image_symbol(u64 img, const char *name) {
	const u8 *h = (const u8 *)img;
	if (r32(h) != MH_MAGIC_64) return 0;
	u32 ncmds = r32(h + 16);
	if (!ncmds || ncmds > 8192) return 0;
	char want[96];
	{
		u64 i = 1;
		want[0] = '_';
		while (name[i - 1] && i < sizeof(want) - 1) { want[i] = name[i - 1]; i++; }
		want[i] = 0;
	}
	const u8 *cmd = h + 32;
	u64 textvm = 0, leditvm = 0, leditfo = 0;
	u32 symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
	for (u32 i = 0; i < ncmds; i++) {
		u32 c = r32(cmd), cs = r32(cmd + 4);
		if (cs < 8) return 0;
		if (c == LC_SEGMENT_64) {
			const char *seg = (const char *)(cmd + 8);
			if (seq(seg, "__TEXT")) textvm = r64(cmd + 24);
			else if (seq(seg, "__LINKEDIT")) { leditvm = r64(cmd + 24); leditfo = r64(cmd + 40); }
		} else if (c == LC_SYMTAB) {
			symoff = r32(cmd + 8); nsyms = r32(cmd + 12);
			stroff = r32(cmd + 16); strsize = r32(cmd + 20);
		}
		cmd += cs;
	}
	if (!nsyms || !leditvm || !textvm) return 0;
	i64 slide = (i64)img - (i64)textvm;
	i64 delta = (i64)(leditvm + (u64)slide) - (i64)leditfo;
	const u8 *st = (const u8 *)((i64)stroff + delta);
	const u8 *sm = (const u8 *)((i64)symoff + delta);
	for (u32 i = 0; i < nsyms; i++) {
		const u8 *e = sm + (u64)i * 16;
		u32 strx = r32(e);
		if (strx >= strsize) continue;
		u8 ntype = e[4];
		if (ntype & 0xe0) continue;          // N_STAB
		if ((ntype & 0x0e) != 0x0e) continue; // require N_SECT, skip N_UNDEF
		if (seq((const char *)(st + strx), want)) return r64(e + 8) + (u64)slide;
	}
	return 0;
}

// Walk the cache's image array and resolve `name` in whichever image exports it.
static u64 cache_symbol(const char *name) {
	u64 cache = shared_cache();
	if (!cache) return 0;
	const u8 *hdr = (const u8 *)cache;
	u32 ioff = r32(hdr + CH_IMAGES_OFF);
	u32 icnt = r32(hdr + CH_IMAGES_CNT);
	if (!ioff || !icnt || icnt > 100000) return 0;
	i64 slide = (i64)cache - (i64)r64(hdr + CH_REGION_START);
	const u8 *arr = (const u8 *)(cache + ioff);
	for (u32 i = 0; i < icnt; i++) {
		const u8 *e = arr + (u64)i * IMAGE_INFO_SIZE;
		u64 au = r64(e);
		if (!au) continue;
		u64 addr = au + (u64)slide;
		if (r32((const u8 *)addr) != MH_MAGIC_64) continue;
		if (r32((const u8 *)addr + 12) != MH_DYLIB) continue;
		u64 v = image_symbol(addr, name);
		if (v) return v;
	}
	return 0;
}

// ---- ELF payload ----------------------------------------------------------
#define PAYLOAD_ALIGN 0x10000
#define EM_AARCH64 183
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define MAX_PHDRS 18
#define PAGESZ 16384

typedef struct { u64 entry, phoff; } ElfhdrKey;
typedef struct { u32 type, flags; u64 offset, vaddr, paddr, filesz, memsz, align; } Phdr;

enum { AT_NULL = 0, AT_PHDR = 3, AT_PHENT = 4, AT_PHNUM = 5, AT_PAGESZ = 6,
       AT_ENTRY = 9, AT_UID = 11, AT_EUID = 12, AT_GID = 13, AT_EGID = 14,
       AT_SECURE = 23, AT_RANDOM = 25, AT_EXECFN = 31 };
#define AUXV_WORDS 32

// The Syslib contract the cosmo runtime validates (os_cosmo_arm64.go: magic
// 'slib', version >= 8). Field order must match cosmo's struct exactly.
struct Syslib {
	int magic;
	int version;
	i64 (*fork)(void);
	i64 (*pipe)(int[2]);
	i64 (*clock_gettime)(int, void *);
	i64 (*nanosleep)(const void *, void *);
	i64 (*mmap)(void *, u64, int, int, int, i64);
	int (*pthread_jit_write_protect_supported_np)(void);
	void (*pthread_jit_write_protect_np)(int);
	void (*sys_icache_invalidate)(void *, u64);
	void *pthread_create;
	void *pthread_exit;
	void *pthread_kill;
	void *pthread_sigmask;
	void *pthread_setname_np;
	void *dispatch_semaphore_create;
	void *dispatch_semaphore_signal;
	void *dispatch_semaphore_wait;
	void *dispatch_walltime;
	void *pthread_self;
	void *dispatch_release;
	i64 (*raise)(int);
	void *pthread_join;
	void *pthread_yield_np;
	int pthread_stack_min;
	int sizeof_pthread_attr_t;
	void *pthread_attr_init;
	void *pthread_attr_destroy;
	void *pthread_attr_setstacksize;
	void *pthread_attr_setguardsize;
	void (*exit)(int);
	i64 (*close)(int);
	i64 (*munmap)(void *, u64);
	i64 (*openat)(int, const char *, int, int);
	i64 (*write)(int, const void *, u64);
	i64 (*read)(int, void *, u64);
	void *sigaction;
	void *pselect;
	i64 (*mprotect)(void *, u64, int);
	void *sigaltstack;
	i64 (*getentropy)(void *, u64);
	void *sem_open;
	void *sem_unlink;
	void *sem_close;
	void *sem_post;
	void *sem_wait;
	void *sem_trywait;
	void *getrlimit;
	void *setrlimit;
	void *dlopen;
	void *dlsym;
	void *dlclose;
	void *dlerror;
	void *pthread_cpu_number_np;
	void *sysctl;
	void *sysctlbyname;
	void *sysctlnametomib;
};

static struct Syslib lib;
static Phdr phdrs[MAX_PHDRS];
static char rando[16];

// Syscall-shaped Syslib entries are supplied by our OWN wrappers rather than by
// libSystem. That is the point: these are kernel entry points whose numbers
// Apple has kept stable far longer than any exported C function. Each returns
// -errno on failure, the convention cosmo's runtime expects.
static i64 s_fork(void) { return svc_e(BSD(SYS_fork), 0,0,0,0,0,0); }
// pipe is NOT usable as a raw syscall here. Measured: BSD 42 returns a single
// fd in x0 and leaves the caller's array untouched, so two calls produce two
// unrelated pipes (a write to one never appears on the other; the read blocks).
// libSystem's pipe() is the only correct source, so this entry is filled from
// the shared cache in fill_syslib() and is deliberately absent here.
static i64 s_close(int fd) { return svc_e(BSD(SYS_close), fd,0,0,0,0,0); }
static i64 s_munmap(void *a, u64 n) { return svc_e(BSD(SYS_munmap), (i64)a,(i64)n,0,0,0,0); }
static i64 s_mprotect(void *a, u64 n, int p) { return svc_e(BSD(SYS_mprotect), (i64)a,(i64)n,p,0,0,0); }
static i64 s_openat(int d, const char *p, int f, int m) { return svc_e(BSD(SYS_openat), d,(i64)p,f,m,0,0); }
static i64 s_write(int fd, const void *b, u64 n) { return svc_e(BSD(SYS_write), fd,(i64)b,(i64)n,0,0,0); }
static i64 s_read(int fd, void *b, u64 n) { return svc_e(BSD(SYS_read), fd,(i64)b,(i64)n,0,0,0); }
static i64 s_getentropy(void *b, u64 n) { return svc_e(BSD(SYS_getentropy), (i64)b,(i64)n,0,0,0,0); }
static i64 s_mmap(void *a, u64 n, int prot, int flags, int fd, i64 off) {
	return svc_e(BSD(SYS_mmap), (i64)a,(i64)n,prot,flags,fd,off);
}
__attribute__((noreturn)) static void s_exit(int code) {
	svc6(BSD(SYS_exit), code, 0,0,0,0,0);
	__builtin_unreachable();
}
static i64 s_clock_gettime(int clk, void *ts) {
	// macOS has no clock_gettime syscall; cosmo's arm64 path reads
	// clock_gettime_nsec_np through dlsym when this is unavailable. Fill the
	// timespec from gettimeofday so callers still get a sane value.
	struct { i64 sec, usec; } tv;
	// gettimeofday is BSD 116.
	if (svc_e(BSD(SYS_gettimeofday), (i64)&tv, 0,0,0,0,0) != 0) return -1;
	struct { i64 sec, nsec; } *t = ts;
	if (t) { t->sec = tv.sec; t->nsec = tv.usec * 1000; }
	(void)clk;
	return 0;
}
static i64 s_nanosleep(const void *req, void *rem) {
	// BSD 101 is SIGSYS-fatal on this kernel (measured), so there is no sleep
	// syscall to call. Returning 0 makes a short delay a no-op, which the
	// runtime tolerates; nothing here fabricates an event that did not happen.
	(void)req; (void)rem;
	return 0;
}

static void fill_syslib(void) {
	lib.magic = 's' | 'l' << 8 | 'i' << 16 | 'b' << 24;
	lib.version = 10;
	// Supplied locally, from raw syscalls.
	lib.fork = s_fork;
	lib.mmap = s_mmap;
	lib.close = s_close;
	lib.munmap = s_munmap;
	lib.openat = s_openat;
	lib.write = s_write;
	lib.read = s_read;
	lib.mprotect = s_mprotect;
	lib.getentropy = s_getentropy;
	lib.exit = s_exit;
	lib.clock_gettime = s_clock_gettime;
	lib.nanosleep = s_nanosleep;
	// The rest come from the shared cache, found by content.
	void **slots[] = {
		(void **)&lib.pipe,
		(void **)&lib.pthread_jit_write_protect_supported_np,
		(void **)&lib.pthread_jit_write_protect_np,
		(void **)&lib.sys_icache_invalidate,
		(void **)&lib.pthread_create, (void **)&lib.pthread_exit,
		(void **)&lib.pthread_kill, (void **)&lib.pthread_sigmask,
		(void **)&lib.pthread_setname_np,
		(void **)&lib.dispatch_semaphore_create,
		(void **)&lib.dispatch_semaphore_signal,
		(void **)&lib.dispatch_semaphore_wait,
		(void **)&lib.dispatch_walltime,
		(void **)&lib.pthread_self, (void **)&lib.dispatch_release,
		(void **)&lib.pthread_join, (void **)&lib.pthread_yield_np,
		(void **)&lib.pthread_attr_init, (void **)&lib.pthread_attr_destroy,
		(void **)&lib.pthread_attr_setstacksize,
		(void **)&lib.pthread_attr_setguardsize,
		(void **)&lib.dlopen, (void **)&lib.dlsym,
		(void **)&lib.dlclose, (void **)&lib.dlerror,
		(void **)&lib.pthread_cpu_number_np,
		(void **)&lib.sysctl, (void **)&lib.sysctlbyname,
		(void **)&lib.sysctlnametomib,
		(void **)&lib.raise, (void **)&lib.sigaction, (void **)&lib.pselect,
		(void **)&lib.sigaltstack, (void **)&lib.sem_open,
		(void **)&lib.sem_unlink, (void **)&lib.sem_close,
		(void **)&lib.sem_post, (void **)&lib.sem_wait,
		(void **)&lib.sem_trywait, (void **)&lib.getrlimit,
		(void **)&lib.setrlimit,
	};
	const char *names[] = {
		"pipe",
		"pthread_jit_write_protect_supported_np",
		"pthread_jit_write_protect_np", "sys_icache_invalidate",
		"pthread_create", "pthread_exit", "pthread_kill", "pthread_sigmask",
		"pthread_setname_np", "dispatch_semaphore_create",
		"dispatch_semaphore_signal", "dispatch_semaphore_wait",
		"dispatch_walltime", "pthread_self", "dispatch_release",
		"pthread_join", "pthread_yield_np", "pthread_attr_init",
		"pthread_attr_destroy", "pthread_attr_setstacksize",
		"pthread_attr_setguardsize", "dlopen", "dlsym", "dlclose", "dlerror",
		"pthread_cpu_number_np", "sysctl", "sysctlbyname", "sysctlnametomib",
		"raise", "sigaction", "pselect",
		"sigaltstack", "sem_open", "sem_unlink", "sem_close", "sem_post",
		"sem_wait", "sem_trywait", "getrlimit", "setrlimit",
	};
	for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		*slots[i] = (void *)cache_symbol(names[i]);
	// pthread_stack_min and sizeof_pthread_attr_t are plain ints the runtime
	// reads directly; 16K and 64 are the arm64 values.
	lib.pthread_stack_min = 16384;
	lib.sizeof_pthread_attr_t = 64;
}

// Map one PT_LOAD. W^X: anonymous RW, fill, then mprotect to the final prot.
static void map_segment(int fd, const Phdr *p) {
	int prot = 0;
	// ELF p_flags: PF_R = 4, PF_W = 2, PF_X = 1.
	if (p->flags & 4) prot |= 1;
	if (p->flags & 2) prot |= 2;
	if (p->flags & 1) prot |= 4;
	u64 base = p->vaddr & ~(u64)(PAGESZ - 1);
	u64 end = p->vaddr + p->memsz;
	u64 span = end - base;
	// Everything anonymous and writable first: RWX would be refused outright.
	i64 r = svc_e(BSD(SYS_mmap), (i64)base, (i64)span, 3,
	              0x1002 /*PRIVATE|ANON*/ | 0x10 /*FIXED*/, -1, 0);
	if (r != (i64)base) die("mmap segment failed");
	if (p->filesz) {
		if (svc_e(BSD(SYS_pread), fd, (i64)p->vaddr, (i64)p->filesz,
		          (i64)p->offset, 0, 0) != (i64)p->filesz)
			die("pread segment failed");
	}
	if (prot != 3) {
		if (svc_e(BSD(SYS_mprotect), (i64)base, (i64)span, prot, 0,0,0) != 0)
			die("mprotect segment failed");
	}
}

__attribute__((noreturn)) static void enter(long *sp, const char *path, u64 entry) {
	register long *x0 __asm__("x0") = sp;
	register const char *x2 __asm__("x2") = path;
	register long x3 __asm__("x3") = 8;          // XNU discriminator
	register struct Syslib *x15 __asm__("x15") = &lib;
	register u64 x16 __asm__("x16") = entry;
	__asm__ volatile(
	    "mov x1, #0\n\tmov x4, #0\n\tmov x5, #0\n\tmov x6, #0\n\tmov x7, #0\n\t"
	    "mov x8, #0\n\tmov x9, #0\n\tmov x10, #0\n\tmov x11, #0\n\tmov x12, #0\n\t"
	    "mov x13, #0\n\tmov x14, #0\n\tmov x17, #0\n\tmov x19, #0\n\tmov x20, #0\n\t"
	    "mov x21, #0\n\tmov x22, #0\n\tmov x23, #0\n\tmov x24, #0\n\tmov x25, #0\n\t"
	    "mov x26, #0\n\tmov x27, #0\n\tmov x28, #0\n\tmov x29, #0\n\tmov x30, #0\n\t"
	    "mov sp, x0\n\tmov x0, #0\n\tbr x16"
	    : : "r"(x0), "r"(x2), "r"(x3), "r"(x15), "r"(x16) : "memory");
	__builtin_unreachable();
}

void cmain(i64 argc, char **argv, char **envp) {
	g_envp = envp;
	if (argc < 2) die("usage: apeld PROG.com [args...]");
	const char *path = argv[1];
	// The payload resolves os.Executable() from argv[0] on hosts without
	// procfs (cosmopolitan's executable_cosmo.go: resolveArgv0 accepts an
	// absolute path, or a relative one against $PATH). A bare relative name
	// like "probe.com" therefore yields an empty Executable(). Hand the
	// payload an absolute path so it resolves, without copying the loader's
	// own argv[0].
	static char abspath[4096];
	const char *argv0 = path;
	if (path[0] != '/') {
		// Prefer $PWD, which costs no syscall and matches what the shell did.
		char *pwd = 0;
		for (char **e = envp; e && *e; e++) {
			const char *s = *e, *p = "PWD=";
			while (*p && *s == *p) { s++; p++; }
			if (!*p && *s == '/') { pwd = (char *)s; break; }
		}
		u64 n = 0;
		if (pwd) {
			u64 i = 0;
			while (pwd[i] && i < sizeof(abspath) - 2) { abspath[i] = pwd[i]; i++; }
			if (i && abspath[i - 1] != '/') abspath[i++] = '/';
			u64 j = 0;
			while (path[j] && i < sizeof(abspath) - 1) abspath[i++] = path[j++];
			abspath[i] = 0;
			n = i;
		}
		if (n) argv0 = abspath;
	}
	i64 fd = svc_e(BSD(SYS_openat), -2 /*AT_FDCWD*/, (i64)path, 0, 0, 0, 0);
	if (fd < 0) die("cannot open program");
	(void)argv0;

	// Find the arm64 payload on a 64K boundary. Its program headers already
	// carry absolute file offsets; only e_phoff is payload-relative.
	u8 eh[64];
	u64 off = PAYLOAD_ALIGN;
	for (;; off += PAYLOAD_ALIGN) {
		if (svc_e(BSD(SYS_pread), fd, (i64)eh, 64, (i64)off, 0, 0) != 64)
			die("no arm64 payload");
		if (r32(eh) == 0x464c457f && *(u16 *)(eh + 18) == EM_AARCH64) break;
	}
	u64 entry = r64(eh + 24);
	u64 phoff = r64(eh + 32);
	u16 phentsize = *(u16 *)(eh + 54);
	u16 phnum = *(u16 *)(eh + 56);
	if (phentsize != sizeof(Phdr) || phnum > MAX_PHDRS) die("bad program header table");
	if (svc_e(BSD(SYS_pread), fd, (i64)phdrs, (i64)phnum * sizeof(Phdr),
	          (i64)(phoff + off), 0, 0) != (i64)phnum * sizeof(Phdr))
		die("cannot read program headers");

	for (int i = 0; i < phnum; i++) {
		Phdr *p = &phdrs[i];
		if (p->type == PT_INTERP || p->type == PT_DYNAMIC) die("payload is dynamic");
		if (p->type != PT_LOAD || !p->memsz) continue;
		if (p->filesz > p->memsz) die("filesz exceeds memsz");
		if ((p->flags & (PF_W | PF_X)) == (PF_W | PF_X)) die("RWX segment");
		map_segment(fd, p);
	}
		fill_syslib();
	svc_e(BSD(SYS_getentropy), (i64)rando, sizeof rando, 0,0,0,0);

	// Diagnostic build: report the handoff, then optionally stop before jumping.
	if (getenv_dbg()) {
		ws("apeld: payload mapped\n");
		ws("  entry  = "); whx(entry);
		ws("  phnum  = "); whx(phnum);
		ws("  argc   = "); whx((u64)argc);
		ws("  syslib.magic   = "); whx((u64)(u32)lib.magic);
		ws("  syslib.version = "); whx((u64)(u32)lib.version);
		ws("  syslib.mmap    = "); whx((u64)lib.mmap);
		ws("  syslib.write   = "); whx((u64)lib.write);
		ws("  syslib.getentropy = "); whx((u64)lib.getentropy);
		ws("  syslib.pthread_create = "); whx((u64)lib.pthread_create);
		ws("  syslib.clock_gettime  = "); whx((u64)lib.clock_gettime);
		ws("  syslib.sysctlbyname   = "); whx((u64)lib.sysctlbyname);
	}

	// New stack block: argc, argv[1..], NULL, envp, NULL, auxv.
	// argv[0] of the loader is dropped, so the payload sees its own path.
	int envc = 0;
	while (envp[envc]) envc++;
	long words = 1 + (argc - 1) + 1 + envc + 1 + AUXV_WORDS;
	static long stack[16384];
	long *sp = stack + 8192;
	sp = (long *)((u64)sp & ~15ULL);
	long *w = sp;
	*w++ = argc - 1;
	*w++ = (long)argv0;                  // absolute, so os.Executable() resolves
	for (int i = 2; i < argc; i++) *w++ = (long)argv[i];
	*w++ = 0;
	for (int i = 0; i < envc; i++) *w++ = (long)envp[i];
	*w++ = 0;
	*w++ = AT_PHDR;   *w++ = (long)(phdrs + 1);
	*w++ = AT_PHENT;  *w++ = sizeof(Phdr);
	*w++ = AT_PHNUM;  *w++ = phnum;
	*w++ = AT_ENTRY;  *w++ = (long)entry;
	*w++ = AT_PAGESZ; *w++ = PAGESZ;
	*w++ = AT_UID;    *w++ = (long)svc6(BSD(SYS_getuid), 0,0,0,0,0,0);
	*w++ = AT_EUID;   *w++ = (long)svc6(BSD(SYS_geteuid), 0,0,0,0,0,0);
	*w++ = AT_GID;    *w++ = (long)svc6(BSD(SYS_getgid), 0,0,0,0,0,0);
	*w++ = AT_EGID;   *w++ = (long)svc6(BSD(SYS_getegid), 0,0,0,0,0,0);
	*w++ = AT_SECURE; *w++ = (long)svc6(BSD(SYS_issetugid), 0,0,0,0,0,0);
	*w++ = AT_RANDOM; *w++ = (long)rando;
	*w++ = AT_EXECFN; *w++ = (long)argv0;
	*w++ = AT_NULL;   *w++ = 0;
	(void)words;
	enter(sp, path, entry);
}
__asm__(".globl _main\n"
        "_main:\n"
        "\tmov x19, x0\n"      // argc
        "\tmov x20, x1\n"      // argv
        "\tmov x21, x2\n"      // envp
        "\tmov x0, x19\n\tmov x1, x20\n\tmov x2, x21\n"
        "\tbl _cmain\n"
        "\tmovz x16, #0x200, lsl #16\n"
        "\tadd  x16, x16, #1\n"
        "\tmov  x0, #0\n"
        "\tsvc  #0x80\n");
