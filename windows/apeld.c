// apeld: boot an APE's amd64 payload on Windows from memory.
//
// The APE is already a valid PE that CreateProcess can run. This loader is
// the in-memory route instead: it reads the file, places the payload at its
// fixed image base by hand, resolves the two-slot import table the cosmo
// runtime boots through (kernel32!GetProcAddress, LoadLibraryA), and calls
// the PE entry point on the current thread. Nothing is written to disk.
//
// Freestanding: no CRT. Only kernel32 is imported.

#include <windows.h>

#define PAYLOAD_ALIGN 0x10000
#define EM_X86_64 62
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define MAX_PHDRS 18

typedef struct {
	unsigned char ident[16];
	WORD type, machine;
	DWORD version;
	ULONGLONG entry, phoff, shoff;
	DWORD flags;
	WORD ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} Ehdr;

typedef struct {
	DWORD type, flags;
	ULONGLONG offset, vaddr, paddr, filesz, memsz, align;
} Phdr;

static HANDLE err;

static void zero(void *p, SIZE_T n) {
	volatile char *c = p;
	while (n--) *c++ = 0;
}

__attribute__((noreturn)) static void die(const char *msg) {
	DWORD n = 0, w;
	while (msg[n]) n++;
	WriteFile(err, "apeld: ", 7, &w, 0);
	WriteFile(err, msg, n, &w, 0);
	WriteFile(err, "\r\n", 2, &w, 0);
	ExitProcess(127);
}

static void pread(HANDLE f, void *buf, DWORD n, ULONGLONG off) {
	OVERLAPPED o;
	zero(&o, sizeof o);
	o.Offset = (DWORD)off;
	o.OffsetHigh = (DWORD)(off >> 32);
	DWORD got = 0;
	if (!ReadFile(f, buf, n, &got, &o) || got != n) die("short read");
}

// Drop the loader's own token from the command line in place, so the
// payload's GetCommandLineW starts at the program path. The buffer is the
// one the PEB holds, so the length there is shortened to match.
static void shift_command_line(void) {
	WCHAR *cl = GetCommandLineW();
	WCHAR *p = cl;
	int quoted = 0;
	while (*p && (quoted || *p != L' ')) {
		if (*p == L'"') quoted = !quoted;
		p++;
	}
	while (*p == L' ') p++;
	if (!*p) die("usage: apeld PROG.com [args...]");
	WCHAR *d = cl;
	while (*p) *d++ = *p++;
	*d = 0;
	// PEB->ProcessParameters->CommandLine is a UNICODE_STRING at +0x70.
	BYTE *peb = (BYTE *)__readgsqword(0x60);
	BYTE *params = *(BYTE **)(peb + 0x20);
	if (*(WCHAR **)(params + 0x78) == cl) *(USHORT *)(params + 0x70) = (USHORT)((d - cl) * 2);
}

static void program_path(WCHAR *out, int cap) {
	WCHAR *p = GetCommandLineW();
	int quoted = 0, n = 0;
	while (*p && (quoted || *p != L' ')) {
		if (*p == L'"') { quoted = !quoted; p++; continue; }
		if (n + 1 >= cap) die("path too long");
		out[n++] = *p++;
	}
	out[n] = 0;
}

static void resolve_imports(BYTE *base, DWORD dir_rva) {
	if (!dir_rva) die("no import directory");
	for (IMAGE_IMPORT_DESCRIPTOR *d = (void *)(base + dir_rva); d->Name; d++) {
		HMODULE m = LoadLibraryA((char *)(base + d->Name));
		if (!m) die("LoadLibrary failed");
		ULONGLONG *names = (void *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
		ULONGLONG *iat = (void *)(base + d->FirstThunk);
		for (; *names; names++, iat++) {
			FARPROC f = (*names & IMAGE_ORDINAL_FLAG64)
			                ? GetProcAddress(m, (char *)(*names & 0xffff))
			                : GetProcAddress(m, ((IMAGE_IMPORT_BY_NAME *)(base + *names))->Name);
			if (!f) die("GetProcAddress failed");
			*iat = (ULONGLONG)f;
		}
	}
}

__attribute__((noreturn)) static void enter(ULONGLONG entry) {
	// Touch the stack downward first: rt0 carves g0's stack from the 64K
	// below the entry SP, and the OS only commits pages through the guard.
	volatile char *sp = (volatile char *)__builtin_frame_address(0);
	for (int i = 1; i <= 40; i++) sp[-i * 4096] = 0;
	// The NT loader calls the entry win64-style: RSP == 8 mod 16, 32 bytes
	// of shadow space above the return address. A call after aligning to
	// 16 and reserving the shadow space gives exactly that.
	__asm__ volatile("and $-16, %%rsp\n\tsub $32, %%rsp\n\tcall *%0" : : "r"(entry) : "memory");
	__builtin_unreachable();
}

__attribute__((noreturn)) void start(void) {
	err = GetStdHandle(STD_ERROR_HANDLE);
	static WCHAR path[32768];
	shift_command_line();
	program_path(path, 32768);

	HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0,
	                       OPEN_EXISTING, 0, 0);
	if (f == INVALID_HANDLE_VALUE) die("cannot open program");

	// PE header at e_lfanew: the entry RVA and import directory RVA live
	// there, not in the ELF payload.
	static BYTE head[0x400];
	pread(f, head, sizeof head, 0);
	if (head[0] != 'M' || head[1] != 'Z') die("not an APE");
	DWORD lfanew = *(DWORD *)(head + 0x3c);
	if (lfanew + 0x108 + 16 > sizeof head || *(DWORD *)(head + lfanew) != 0x4550) die("bad PE header");
	BYTE *opt = head + lfanew + 24;
	if (*(WORD *)opt != 0x20b) die("not PE32+");
	DWORD entry_rva = *(DWORD *)(opt + 16);
	ULONGLONG image_base = *(ULONGLONG *)(opt + 24);
	DWORD import_rva = *(DWORD *)(opt + 112 + 8);

	// The amd64 ELF payload on a 64K boundary. Its p_offsets are absolute.
	Ehdr eh;
	ULONGLONG off = PAYLOAD_ALIGN;
	for (;; off += PAYLOAD_ALIGN) {
		LARGE_INTEGER sz;
		if (!GetFileSizeEx(f, &sz) || off + sizeof eh > (ULONGLONG)sz.QuadPart) die("no amd64 payload");
		pread(f, &eh, sizeof eh, off);
		if (*(DWORD *)eh.ident == 0x464c457f && eh.machine == EM_X86_64) break;
	}
	if (eh.phentsize != sizeof(Phdr) || eh.phnum > MAX_PHDRS) die("bad program header table");
	static Phdr ph[MAX_PHDRS];
	pread(f, ph, eh.phnum * sizeof(Phdr), eh.phoff + off);

	ULONGLONG lo = ~0ULL, hi = 0;
	for (int i = 0; i < eh.phnum; i++) {
		if (ph[i].type != PT_LOAD || !ph[i].memsz) continue;
		if (ph[i].vaddr < lo) lo = ph[i].vaddr;
		if (ph[i].vaddr + ph[i].memsz > hi) hi = ph[i].vaddr + ph[i].memsz;
	}
	if (lo != image_base) die("payload base differs from PE image base");
	lo &= ~0xfffULL;
	hi = (hi + 0xfff) & ~0xfffULL;
	// The image is not relocatable, so the range must be exactly this one.
	BYTE *base = VirtualAlloc((void *)lo, hi - lo, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (base != (BYTE *)lo) die("image base is not free");

	for (int i = 0; i < eh.phnum; i++) {
		if (ph[i].type != PT_LOAD || !ph[i].filesz) continue;
		pread(f, (void *)ph[i].vaddr, (DWORD)ph[i].filesz, ph[i].offset);
	}
	CloseHandle(f);

	resolve_imports(base, import_rva);

	for (int i = 0; i < eh.phnum; i++) {
		if (ph[i].type != PT_LOAD || !ph[i].memsz) continue;
		DWORD prot = (ph[i].flags & PF_X) ? PAGE_EXECUTE_READ : (ph[i].flags & PF_W) ? PAGE_READWRITE : PAGE_READONLY;
		DWORD old;
		ULONGLONG a = ph[i].vaddr & ~0xfffULL;
		ULONGLONG b = (ph[i].vaddr + ph[i].memsz + 0xfff) & ~0xfffULL;
		if (!VirtualProtect((void *)a, b - a, prot, &old)) die("VirtualProtect failed");
	}
	if (!FlushInstructionCache(GetCurrentProcess(), base, hi - lo)) die("FlushInstructionCache failed");

	enter(image_base + entry_rva);
}
