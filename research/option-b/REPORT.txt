# Option B: minimizing the dependency surface — findings

Environment: macOS 26.5 (Darwin 25.5.0), Apple M5, arm64. All numbers below were measured on this machine with `/usr/bin/clang`, `/usr/bin/ld`, `otool`, `nm`, `dyld_info`, `mincore`. Scratch dir `/tmp/apex-b`. Nothing outside it was modified. Every program in `SOURCES/` builds with the exact commands in `BUILD.md`.

---

#Bottom line

**Option B is necessary but NOT sufficient.** The zero-import bootstrap works — I built it and it resolves `malloc`, `pthread_mutex_lock`, `dlsym`, etc. with `nm -u` reporting **zero undefined symbols**. But the stated goal (surviving an Apple that renames `libSystem.B.dylib`) is **not reached**, for a reason that has nothing to do with symbol names:

> **dyld itself dies before `main` when none of the process's dylib references > resolve.** The failure is `dyld[N]: libdyld.dylib not found`, then `SIGABRT`. > It happens in the loader, before a single instruction of ours runs. No > amount of syscall rewriting or symbol-table walking can defend against it. > Note this is *not* about symbol names: `LC_LOAD_WEAK_DYLIB` makes a **symbol** > optional. It does not make **the whole library set** optional.

Every zero-import binary nevertheless **must** carry at least one `LC_LOAD_DYLIB`/`LC_LOAD_WEAK_DYLIB` (Apple's ld refuses to link without it: `dynamic executables or dylibs must link with libSystem.dylib`). That mandatory name reference is the irreducible exposure. And it is the one thing Option B was meant to remove.

The mitigation I found and verified is a **hedge dylib**: carry a second weak `LC_LOAD_WEAK_DYLIB` pointing at a library we control. The binary then survives `libSystem` being absent, provided the hedge resolves (see §4). Verified: with `/usr/lib/libSystem.B.dylib` replaced by an absent name, the binary **runs** with the hedge present and **SIGABRTs** with the hedge missing.

---

#Raw-syscall mapping


Three corrections to the stated ground truth, all measured:

| Claim (given) | Measured |
|---|---|
| BSD = `0x2000000\|n`, mach = `0x1000000\|n` in x16 | **Class tags are ignored for BSD.** `x16=20` → `getpid` works; `x16=0x2000014` → also `getpid`; `x16=0x1000014` → identical. The kernel masks the class bits. |
| (same) for mach traps | **Mach traps are NOT `0x1000000\|n`.** They use a **negative** x16: libSystem's `task_self_trap` stub is `MOVN x16,#27` then `svc`, i.e. **x16 = −28**. Verified end-to-end: `x16=−28` returns **515**, which equals `mach_task_self()` from libc. |
| "classes 1–4 work, only class 0 traps" | The real rule is on the **value**: some low numbers trap, others return. See §1.2. |

Evidence: `p_class.c` (calls `svc` with an arbitrary x16 and prints the result) scanned x16 = 0..80 (`logs/class_scan.txt`).`mt2.c`/`mt3.c` decode and exercise the `MOVN` mach-traps.`p_stubs2.c` dumps the libSystem stubs that revealed the encoding.

**Return convention also differs.** BSD syscalls signal errors with the carry flag (negate x0 → `-errno`). **Mach traps do not.** They return their result directly in x0. Verified in `mt3.c`: calling `task_self_trap` raw four times returns `515` each time with no carry handling. A loader needs **two** entry points — one that negates on carry for BSD, one that does not for mach traps. Applying the BSD convention to a mach trap will corrupt any return whose high bit is set.

### 1.2 Which x16 values are fatal

| x16 value | result |
|---|---|
| `0` | **SIGSYS (fatal)** |
| `1` | exit; no output |
| `2..` | ordinary syscalls (note `40` and `44` are **SIGSYS**) |
| `101` (would be `nanosleep`) | **SIGSYS (fatal)** |
| `9999` | SIGSYS (out of range) |
| negative (mach traps) | return normally, e.g. `−28` → 515 |

So `x16 = 0` is the guaranteed-fatal value, and there is a scattering of individual numbers (40, 44, 62–64, 66, 69–72, 76–77, 87 …) that also raise SIGSYS. **Do not treat "invalid number returns an error" as a general rule** — probe each number before shipping it. My earlier assumption that 101 was safe was wrong and cost a build cycle.`nanosleep` is fatal at this trap level. (Full scan in `logs/class_scan.txt`.)

### 1.3 Function → mechanism table

Legend: **[RAW]** = our own `svc`, ABI-stable, no Apple name · **[PTR]** = must resolve a libSystem function pointer · **[NONE]** = not obtainable in a stable way.

| Syslib entry | Mechanism | Raw number / note |
|---|---|---|
| `mmap` | **[RAW]** | BSD 197. Verified anon + file mapping. |
| `mprotect` | **[RAW]** | BSD 74. Verified. |
| `munmap` | **[RAW]** | BSD 73. Verified. |
| `task_self_trap` (for `mach_vm_*`) | **[RAW mach]** | mach trap **x16 = −28**, verified returns 515. Needed if a caller wants `mach_vm_region`-style enumeration without importing it. |
| `mach_vm_region` | **[PTR or mach DIY]** | Real C (`mach_vm_region` is a fat stub with `stp`/`sub sp`). The underlying trap is `_kernelrpc_mach_vm_region_trap`-class; building the `mach_msg`/reply-port dance by hand is possible but fragile. Resolve the pointer. |
| `host_self_trap` / `thread_self_trap` / `mach_reply_port` | **[RAW mach]** | x16 = −29 / −27 / −26, all verified to return small port names. |
| `pread` | **[RAW]** | BSD 153. Verified (returned 16 bytes from a file). |
| `pwrite` | **[RAW]** | BSD 154. |
| `openat` | **[RAW]** | BSD 463. |
| `close` | **[RAW]** | BSD 6. |
| `write` | **[RAW]** | BSD 4. |
| `read` | **[RAW]** | BSD 3. |
| `getentropy` | **[RAW]** | BSD 500. Verified: returns 0 and fills the buffer. |
| `sysctl` | **[RAW]** | BSD 202. Verified `{CTL_HW,HW_NCPU}` and `{CTL_KERN,KERN_HOSTNAME}`. **`sysctlbyname` has no syscall** — use the MIB form, or `sysctlnametomib`'s job done by hand. |
| `getrlimit` / `setrlimit` | **[RAW]** | BSD 194 / 195. Verified. |
| `sigaltstack` | **[RAW]** | BSD 53. Verified set + query. |
| `pselect` | **[RAW]** | BSD 394. Verified with a zero timeout (returns 0). |
| `select` | **[RAW]** | BSD 93. Usable as a `nanosleep` substitute (verified). |
| `fork` | **[RAW — semantics differ]** | BSD 2. **See §1.4.** |
| `pipe` | **[RAW — semantics differ]** | BSD 42. **See §1.4.** |
| `exit`/`_exit` | **[RAW]** | BSD 1. |
| `issetugid` | **[RAW]** | BSD 327. Verified. |
| `thread_selfid` | **[RAW]** | BSD 372 — but this is a **kernel** tid (small integer), *not* a pthread_t. Not a substitute for `pthread_self`. |
| `clock_gettime` | **[PTR]** | No BSD number. `gettimeofday` (BSD 116) gives wall time; there is **no raw monotonic clock**. |
| `nanosleep` | **[RAW alt]** | BSD 101 is **SIGSYS**. Use `select(93)` with a zeroed `timeval`, or `pselect`. Verified. |
| `sigaction` | **[PTR]** | BSD 46 exists and returns 0, **but the handler is never entered** — see §1.5. |
| `raise` | **[PTR]** | Built on kill/`__pthread_kill`; wrappers differ per thread. |
| `pthread_*` | **[PTR]** | Real C, not syscalls. See §1.6. |
| `dispatch_*` | **[PTR]** | libdispatch C runtime. |
| `sem_open/unlink/close/post/wait/trywait` | **[PTR in general]** | BSD 268–273 exist and are thin `svc` wrappers (`sem_post` etc. disassemble to `movz x16,#0x223; svc`). **But** `sem_open` is real C (it creates a named POSIX semaphore object), and the whole family operates on Mach semaphores — reimplementing the protocol is out of scope. Use the libc pointers. |
| `sysctlbyname` | **[PTR or DIY]** | No syscall. Either resolve the pointer, or hand-build the MIB (see §1.3). |
| `pthread_cpu_number_np` | **[PTR]** | Real C. |
| `sys_icache_invalidate` | **[PTR]** | Real C (uses `dc/ic` ops + barriers). |
| `pthread_jit_write_protect_{np,supported_np}` | **[PTR]** | Real C (uses the APRR system registers). |
| `dlopen` / `dlsym` / `dlclose` / `dlerror` | **[PTR or REPLACE]** | See §2 — I built a drop-in replacement. |


**`fork` (BSD 2) does not return 0 in the child.** The raw kernel call returns the *child's pid in both processes*. The discrimination must be done by comparing `getpid()` before and after:

```
p0=34223 p1=34223 ppid=34214 fork_x0=34224 is_child=0   <- parent
p0=34223 p1=34224 ppid=34223 fork_x0=34224 is_child=1   <- child
```

libc's `fork()` wrapper is what converts this to the POSIX `0`-in-child contract (verified: `fork()` gives `fork_rc=0` in the child). Any loader that wraps raw `fork` for the payload **must** apply that conversion or the payload's child will believe it is the parent. (`syscall(SYS_fork)` reproduces the raw behaviour, confirming this is the kernel, not my asm.)

**`pipe` (BSD 42) leaves `fd[1]` untouched.** With `int fds[2] = {-7,-7}`, raw `pipe` returned `rc=4` and `fds = {4, -7}`. libSystem's `pipe()` wrapper writes both entries. A loader wrapping this naively hands the payload a garbage write fd. `forkchk.c`, `p_raw.c` reproduce both.


`svc` BSD 46 installs (returns 0) and even round-trips through a query, but **the handler is never invoked** for SIGSEGV/SIGBUS (null-deref, unmapped read) or for a software `kill`. libc's `sigaction` works for the same program. Cause: the kernel needs the trampoline/restorer field that libc supplies (Apple's `struct sigaction` is 16 bytes. The kernel-side structure is larger). And the kernel will not synthesise a return path for a handler we install ourselves. Practical consequence: **a memory-safety scanner cannot use signals** — it must use `mincore` polling instead (see §2.3).

### 1.6 Do not reimplement pthread primitives

I disassembled every wrapper. `pthread_mutex_lock`, `pthread_cond_wait`, `pthread_create`, `pthread_join`, `pthread_sigmask`, `pthread_setname_np` are **real C functions with real frames** (`stp`/`sub sp`/`bl`), not `svc` stubs. They sit on `__psynch_*` syscalls (301–312), `bsdthread_*` (360+), `__semwait_signal` (334) and Mach ports. Those are a private contract: Apple changes the kernel/user structs and the handshake between libpthread and XNU.

**Blunt warning:** reimplementing `pthread_mutex_*`/`pthread_cond_*` on `__psynch_mutexwait`/`__psynch_cvwait` is **reckless**, not clever. It will appear to work in a smoke test and then corrupt under contention, on thread teardown, or after an OS update. The consumer runtime already reaches the same conclusion for a weaker reason (`os_cosmo_arm64_sema.go`: *"Never park an M on a Syslib dispatch semaphore"*) and parks Ms on **dlsym'd** `pthread_mutex_t`/ `pthread_cond_t` precisely because the pthread objects must stay opaque.

The correct Option-B posture here: **do not reimplement, resolve the pointer.**

---



No import, no `dlsym`, no `LC_SYMTAB` fixups, no `_dyld_*` symbol. Just raw `mincore` + pointer arithmetic:

1. **Find the dyld shared cache header.** Scan the mapped region for the ASCII magic `dyld_v1` at page granularity, guarded by `mincore`. On this machine the primary cache header is at `0x188830000` — **identical across every run** (checked 8×). The arm64e shared cache is **not** slid per process here. The slide is `0x8830000` every time. *This must be treated as an observation, not a guarantee — see §2.5.*

2. **Read the cache's image-text table.** Header offset `+0x88` = `imagesTextOffset`, `+0x90` = `imagesTextCount`. Each 32-byte entry is `{uuid[16]; loadAddress u64; textSegmentSize u32; pathFileOffset u32}`. Verified: entry 0 → `/usr/lib/libobjc.A.dylib`, entry 1 → `/usr/lib/system/libdyld.dylib`, etc.

3. **Map install-name → runtime address:** `runtime = entry.loadAddress + slide` where `slide = cachebase − 0x180000000`.

4. **Walk the export trie.** `LC_DYLD_EXPORTS_TRIE` (`0x80000033`) gives `dataoff`/`datasize`. **The trie address needs `__LINKEDIT`'s own `vmaddr`/`fileoff`** because in the split-cache layout `__LINKEDIT` lives in a *different subcache* than `__TEXT`:

   ```
   trie   = __LINKEDIT.vmaddr + slide + (dataoff − __LINKEDIT.fileoff)
   symbol = image_header + export_offset          // offsets are image-relative
   ```

Getting this wrong (using `image_header + dataoff`) silently reads another symbol's bytes — that was my first failure mode.

5. Trie format: nodes are `uleb terminalSize`, terminal payload `uleb flags`, then either `uleb address` (normal) or ordinal + C-string (re-export, flag `0x08`). Edges are `NUL-terminated chars` + `uleb childOffset`. **Edge strings include the leading `_`** (`_getpid`, not `getpid`).

### 2.2 Verification against ground truth

Every address the resolver produced is byte-identical to `dlsym(RTLD_DEFAULT,…)` in the same session:

| symbol | resolver | `dlsym` |
|---|---|---|
| `_getpid` | `0x188cde178` | `0x188cde178` |
| `_mmap` | `0x188cde96c` | `0x188cde96c` |
| `_munmap` | `0x188ce16b8` | `0x188ce16b8` |
| `_mprotect` | `0x188ce1afc` | `0x188ce1afc` |
| `_mach_vm_region` | `0x188ce2824` | `0x188ce2824` |
| `_openat` | `0x188cfa18c` | `0x188cfa18c` |
| `_write` | `0x188ce182c` | `0x188ce182c` |
| `_read` | `0x188cde914` | `0x188cde914` |
| `_pselect` | `0x188cf885c` | `0x188cf885c` |
| `_pipe` | `0x188ce43e0` | `0x188ce43e0` |
| `_getrlimit` | `0x188ce00b8` | `0x188ce00b8` |
| `_setrlimit` | `0x188ce0a00` | `0x188ce0a00` |
| `_sem_open` | `0x188ceaa10` | `0x188ceaa10` |
| `_pthread_self` | `0x188d1d590` | `0x188d1d590` |
| `_pthread_create` | `0x188d1e768` | `0x188d1e768` |
| `_pthread_kill` | `0x188d217b0` | `0x188d217b0` |
| `_pthread_sigmask` | `0x188d21078` | `0x188d21078` |
| `_pthread_attr_*` (4) | ✓ | ✓ |
| `_dlsym` | `0x188910c04` | `0x188910c04` |
| `_dlopen` | `0x188910b30` | `0x188910b30` |
| `_malloc` | `0x188b14ce8` | `0x188b14ce8` |
| `_pthread_mutex_lock` | `0x188d1c3fc` | `0x188d1c3fc` |
| `_pthread_cond_wait` | `0x188d1e9c0` | `0x188d1e9c0` |
| `_pthread_cond_timedwait_relative_np` | `0x188d206a4` | `0x188d206a4` |

`nm -u` on the resolver binary: **empty**. It also *calls* `getpid()` and `pthread_self()` through resolved pointers successfully.

**The chicken-and-egg problem is solved.** This resolver can *be* the loader's `dlsym`. So the loader needs no `dlsym` import to obtain one.


`mincore` (BSD 78) is the safe probe, with the **inverted** convention on XNU:

- `vec[0] & 0x80` **set** → page is **NOT** mapped
- `vec[0] & 0x80` **clear** → mapped

I initially assumed the opposite and bus-errored on unmapped pages. Confirmed by fork-per-probe (`probesafe.c`): `0x180000000` reads FAULT with `vec0=0x80`.`0x188830000` reads OK with `vec0=0x03`. Note `sysctl vm.region`-style enumeration is unnecessary if you scan with `mincore`, which is fortunate, because a mach trap enumeration route will need hand-built `mach_msg` headers.


- 308 Mach-O images found by a full page scan, 512 in a wider window.
- Full scan is **slow** — the naive version times out (>120 s) because every node visit costs several `mincore` syscalls. The cache image table removes that: name lookup is ~3646 pointer-chases, and **prefix pruning** in the trie walk makes a single-symbol resolve milliseconds.
- **Re-exported symbols are not in their own image's trie** (`_fork`, `_getentropy`, `_sigaction`, `_raise` all resolved to 0 via the primary route). A global fallback that tries every image's trie finds them. But that is the slow path — measure it (it was >280 s unpruned in my runs). **Apply prefix pruning before adopting the global fallback.**


- **Proven:** the algorithm and the address model, against `dlsym` ground truth, on this OS build.
- **Assumed, and load-bearing:** that the cache layout fields (`+0x88`/`+0x90`, entry stride 32, the `__LINKEDIT`-relative trie formula) are stable across macOS 11–26. They are documented dyld internals, not a public contract. The `0x8830000` slide being constant across runs is this machine's behaviour. I did not test whether ASLR can vary it. And a resolver must compute the slide from a known image rather than hard-code it.
- **Not tested:** a real macOS 11 Big Sur runtime (this box is 26.5).

---

## 3. The minimal viable Syslib table

### 3.1 What the consumer actually reads

From `rt0_cosmo_arm64.s` and `os_cosmo_arm64.go`, the **hard gate** is:

```go
if lib != nil && lib.magic == _SYSLIB_MAGIC && lib.version >= 8 { return }
writeErrStr("runtime: APE loader Syslib is missing or too old (need v8+); ...")
exit(127)
```

So the loader must set `magic = "slib"` and `version >= 8` **and the struct must physically extend through at least `dlerror`**. A v8 loader can leave v9/v10 fields (`pthread_cpu_number_np`, `sysctl*`) zero — those are version-gated at their use sites and degrade gracefully (`cosmoDarwinNumCPU` returns 0, `cosmoDarwinHostname` returns "", `cosmoDarwinSysctlEnabled` returns false).

### 3.2 Boot path vs lazy

**Boot path (read during `osinit` → `osArchInit`, or unconditionally on first thread/signal use):**

| entry | offset | why it is needed |
|---|---|---|
| `magic` / `version` | 0 / 4 | `cosmoCheckSyslib` gate; also `rt0` checks magic |
| `dlsym` | 392 | `osArchInit` resolves ~60 host functions with it, **including getpid/getppid/getuid/… and the whole pthread mutex/cond set** |
| `pthread_self` | 144 | `minitProcid` — `minit` calls it on *every* M |
| `pthread_create` | 72 | `clone` (new M). Go's scheduler needs this immediately. |
| `pthread_kill` | 88 | `darwinSignalM` (async preemption) |
| `pthread_sigmask` | 96 | `darwinSigprocmask` (initsig) |
| `sigaltstack` | 304 | `darwinSigaltstack` (initsig) |
| `sigaction` | 280 | `darwinSigaction` (initsig) |
| `exit` | 232 | runtime exit paths |
| `write` | 264 | `writeErrStr`; also the v8 check's own error message |
| `read` / `close` / `openat` | 272 / 240 / 256 | `readRandom` (boot hash seed) |
| `clock_gettime` | 24 | timekeeping from the first `nanotime` |
| `pselect` | 288 | `sigNote` pipe waits (netpoller / signal note) |
| `mmap` | 40 | allocator |
| `munmap` / `mprotect` | 248 / 296 | allocator |
| `pipe` | 16 | `pipe2` (nonblockingPipe for the netpoller) |
| `nanosleep` | 32 | sleeps |
| `raise` | 160 | crash relay |
| `getentropy` | 312 | startup randomness |
| `pthread_attr_*` | 200–224 | thread creation |
| `pthread_join` / `pthread_exit` / `pthread_yield_np` | 168 / 80 / 176 | lifecycle |
| `getrlimit` / `setrlimit` | 368 / 376 | `sysargs`/`stack limits` |
| `sem_*` (6) | 320–360 | cosmo libc / payload use (not Go runtime) |
| `dispatch_*` (4) | 112–136 | cosmo libc / payload use |
| `jit_write_protect*`, `icache` | 48–64 | cosmo libc / JIT payload use |
| `dlopen`/`dlclose`/`dlerror` | 384 / 400 / 408 | cosmo libc / payload use |

**Lazy / optional (safe to zero):** `pthread_cpu_number_np` (416), `sysctl` (424), `sysctlbyname` (432), `sysctlnametomib` (440).

### 3.3 Do missing entries degrade gracefully?

**Partly — and this is the sharp edge.** Two independent behaviours:

- **Zero-tolerant** (explicit `if (lib == nil || lib.X == 0) return …`): `pthread_*(attr/join/kill/sigmask)`, all `sem_*`, all `dispatch_*`, `dl*`, `sys_*`, `mprotect`, `sigaltstack`, `sigaction`, `getentropy`, `pthread_cpu_number_np`, `sysctl*`. These are safe to leave 0.
- **Zero-fatal** (dereferenced or called with no null check): `write` (the error path itself — the comment in `cosmoCheckSyslib` admits the message may be lost), `dlsym` (gated only on `version >= 6`), and the assembly paths that jump straight to `MOVD off(R9), R12` without a `CBZ`.

`os_cosmo_arm64_sema.go:semacreate` is explicit about the intent:

> *"cosmoSemaInit runs from osinit, before any M can park. A miss here means > libSystem stopped exporting a pthread symbol. Dying loudly beats parking on > garbage."* — it `throw()`s if any of the seven pthread mutex/cond pointers are 0.

**Design consequence:** a loader that resolves lazily must still resolve `dlsym` **eagerly**, because that single entry is what the runtime uses to obtain everything the Syslib does not export (including the pthread mutex/cond primitives that keep the scheduler alive). A lazily-empty Syslib will fail at `osArchInit`, not later.

### 3.4 Minimum honest table

| tier | count | entries |
|---|---|---|
| **Tier 0 — hard minimum to reach `main`** | 2 fields | `magic`, `version >= 8` (rest zero is *not* safe: `dlsym` and `write` are needed before any Go code runs) |
| **Tier 1 — minimum that boots a Go/cosmo payload** | ~18 | `dlsym`, `write`, `close`, `openat`, `read`, `mmap`, `munmap`, `mprotect`, `clock_gettime`, `pselect`, `pipe`, `nanosleep`, `pthread_self`, `pthread_create`, `pthread_kill`, `pthread_sigmask`, `sigaction`, `sigaltstack` |
| **Tier 2 — full v10 surface** | 55 | all of §1.3 |

With the §2 resolver, **every one of Tier 1/2 is obtainable with zero imports**. The only entries with no raw-syscall path are `pthread_*`, `dispatch_*`, `dl*`, `sys_icache_invalidate`, `jit_*`, and `clock_gettime` — all resolvable.

---

## 4. Weak-link robustness matrix

Measured with stub dylibs built on this machine (`A`–`E` in `/tmp/apex-b`), plus in-place install-name patching of real binaries (patched name kept the same length.`codesign -f -s -` re-run after each edit).

| # | binary's dylib references | library state | result |
|---|---|---|---|
| 1 | strong `libSystem.B.dylib` | present | **runs** (exit 42) |
| 2 | strong `libSystem.B.dylib` | absent (renamed) | **SIGABRT**, `Library not loaded: /usr/lib/libMacosX.B.dylib` |
| 3 | weak only `libSystem.B.dylib` | present | **runs** |
| 4 | weak only `libSystem.B.dylib` | absent (renamed) | **SIGABRT**, `libdyld.dylib not found` ← *contradicts the "weak tolerates absence" expectation* |
| 5 | weak only `libMacos.B.dylib` (absent) | — | **SIGABRT**, `libdyld.dylib not found` |
| 6 | weak `libMacos.B.dylib` + weak `libNope.1.dylib` (both absent) | — | **SIGABRT**, `libdyld.dylib not found` |
| 7 | weak `libSystem.B.dylib` + weak `libz.1.dylib` (present) | libSystem renamed | **runs** |
| 8 | weak `libSystem.B.dylib` + weak `libz.1.dylib` (present) | both renamed | **SIGKILL 9** |
| 9 | weak `libSystem.B.dylib` + weak `@loader_path/libhedge.dylib` (present) | libSystem renamed | **runs** |
| 10 | same as 9, but hedge file missing | libSystem renamed | **SIGABRT**, `libdyld.dylib not found` |
| 11 | same as 9, but hedge file replaced by non-Mach-O text | libSystem renamed | **SIGABRT** |
| 12 | weak `libMacos.B.dylib` + **strong** `libSystem.B.dylib` | — | **runs** (strong wins, weak ignored) |
| 13 | no dylib reference at all | — | **won't link**: `dynamic executables or dylibs must link with libSystem.dylib` |

### What this means

1. **"At least one `LC_LOAD_WEAK_DYLIB` must resolve" is the real rule.** A binary whose only dylib references are all absent dies in dyld (`libdyld.dylib not found`) regardless of strong-vs-weak. `LC_LOAD_WEAK_DYLIB` makes a *symbol* optional. It does not make *the whole library set* optional. Note case 6: two absent weak libs still abort. So it is not merely "count > 1".

2. **Strong is strictly worse** for this purpose (case 2 vs 4): strong aborts with a name-specific message. Weak at least gets as far as the generic `libdyld.dylib` abort. Neither survives. For robustness, weak is the right choice, but it buys nothing against a rename.

3. **The hedge dylib is the only working mitigation I found** (cases 9–11). It costs a second `LC_LOAD_WEAK_DYLIB` (8 bytes of load command) and works only while the hedge file is present, a valid Mach-O, and itself links. Cases 7/8 show the same effect using system `libz` — but `libz` is also an Apple-controlled name. So it is a poor hedge. A hedge we ship is better. A hedge we ship is a file that "an Apple that breaks things deliberately" can also delete. So this is mitigation, not immunity.

---

## 5. minos and dialect

Measured with `ld -platform_version macos X X`:

| minos | size | `LC_DYLD_CHAINED_FIXUPS` | `LC_DYLD_INFO_ONLY` |
|---|---|---|---|
| 10.15 | 16840 | absent | **present** |
| **11.0** | **16840** | absent | **present** |
| 12.0 | 16888 | **present** | absent |
| 13.0 | 16888 | present | absent |
| 14.0 | 16888 | present | absent |
| 15.0 | 16888 | present | absent |
| 26.0 | 16888 | present | absent |

- **Confirmed: minos ≤ 11.0 emits the old dyld dialect** (`LC_DYLD_INFO_ONLY`, no chained fixups). 12.0 is the switchover. Targeting 11.0 is correct for a Big Sur floor and is what the linker does naturally.
- **Cost of 11.0: none measured.** A minos-11.0 zero-import weak-linked binary runs correctly on macOS 26.5 (`hi`, exit 0). No load commands go missing that matter. The 48-byte size difference is the chained-fixups load command itself.
- **Independently confirmed:** a zero-import weak-linked binary has literally **zero bind fixups** — `bind_off = bind_size = weak_bind_off = lazy_bind_off = 0` in `LC_DYLD_INFO_ONLY`. The only non-zero field is `export_off`. So there is nothing for dyld to bind, and `nm -u` is empty. This is the strongest form of "no imported symbols" available.

---

## 6. Size

All arm64 binaries here are padded to 16 KiB pages (Apple arm64 page size).

| program | file size | segments | real code/data |
|---|---|---|---|
| `min` (raw `svc`, print, exit) | **16 832** | `__TEXT` 16 K + `__LINKEDIT` 16 K | `__text` 136 B + `__cstring` 4 B |
| full zero-import resolver (`zfinal`) | **33 448** | `__TEXT` + `__DATA_CONST` + `__DATA` + `__LINKEDIT` | `__text` 3 336 B, `__cstring` 1 036 B, `__const` 848 B, `__bss` 16 B |
| resolver + drop-in `dlsym` (`mydlsym`) | 33 480 | same | `__text` 3 732 B, `__cstring` 284 B |

Breakdown of the resolver's 33 448 bytes:

- **32 768 B (98%) is unavoidable 16 KiB page padding** — four segments × 16 KiB.
- **680 B is the `__LINKEDIT` payload** (symbol table, export trie, code signature, function starts).
- Real instruction bytes: **~3.3 KB**.

Size levers I tried, all of which **did not help** on arm64: `-dead_strip`, `-S`, `strip -S`, `-no_pie` (ignored: `-no_pie ignored for arm64*`). `-no_data_const` moves `__DATA_CONST` into `__DATA` and saves nothing (same file size, same page count). The 16 KiB floor per segment is hard.

Practical target: **a fully zero-import APE loader must land at 48–64 KiB** (one `__TEXT` page for code + headers, one `__LINKEDIT` page, and one or two more pages if you keep mutable state out of `__TEXT`). The current tree loader `darwin/apeld.c` is 36 944 B with 67 undefined symbols, so Option B is **already size-competitive while importing nothing**.

---

#Recommendation

**Option B alone does not reach the stated goal.** The hard blocker is Apple's *install-name* namespace, not the symbol namespace. And it is enforced by dyld before our code runs:

- You cannot link without naming at least one dylib.
- If the only named dylib fails to resolve, the process is `SIGABRT`ed in dyld.
- Renaming `libSystem.B.dylib` therefore kills every loader — including one with zero imported symbols — unless a second, resolving weak dylib is present.

**What Option B does buy. And it is real:**

1. **Immunity to symbol-level churn.** Symbol removal, ABI change, or a `libc`/`libSystem` reorganisation that keeps the install name cannot break a zero-import loader. All 55 Syslib functions are obtainable by walking the export trie. And the loader can *be* `dlsym` for the payload.
2. **Immunity to bind-time failures.** Zero bind fixups means dyld has no symbol work to do on our behalf at all.
3. **A size win.** 33 KB with zero imports vs 37 KB with 67 — and the size is dominated by page padding either way.
4. **A graceful floor.** Entries the runtime does not strictly need degrade to 0 by design. Only `dlsym` and `write` are genuinely load-bearing-and-fatal.

**What to do, concretely:**

- **Adopt** the §2 resolver as the loader's symbol source, with prefix pruning and the global re-export fallback.
- **Adopt** raw syscalls for everything in §1.3 marked `[RAW]`, **with the two semantic fixes**: `fork` child-detection by `getpid()` comparison, and `pipe` writing both fd slots. Do not ship either raw call unfixed.
- **Use `select`/`pselect` for sleeping**. Never `x16 = 101`.
- **Resolve, do not reimplement,** `pthread_*` and `dispatch_*`.
- **Ship the hedge dylib** as `@loader_path`-relative and weak. It is the only mitigation that survives a `libSystem` rename. And it costs bytes.
- **Keep minos at 11.0** (verified good, old dialect, no downside).

**Where Option A is still required.** Option A's content-based discovery is the only route that removes the mandatory install-name reference — i.e. it is the only thing that answers "Apple renames `libSystem`". And it is worth being precise about the limit even there: I found no way for a *Mach-O* to avoid `LC_LOAD_DYLIB` entirely (Apple's `ld` refuses). The escape has to go **around** Mach-O — an existing process's memory, a shell/bootstrap in another form, or whatever Option A concluded. Option B must be adopted as the *inner* mechanism (it makes the loader's own dependency surface disappear). Option A kept as the *outer* mechanism that gets the loaded image past dyld's install-name check.

**Flagged risks, bluntly:**

- Walking dyld-internal structures (cache header offsets, trie encoding, `__LINKEDIT`-relative addressing) trades a *public, named* dependency for an *internal, unnamed* one. That is the correct trade against a hostile Apple (internal layouts are harder to break deliberately than a rename. But they do change between releases). Budget for per-OS-version validation.
- The `0x8830000` slide and `0x188830000` cache base being constant is this machine/OS's behaviour. Do **not** hard-code them. Derive the slide.
- `mincore` convention (`0x80` = unmapped) is inverted from the usual reading and cost me a bus error. Document it.
- Raw `fork` returning child-pid-in-child is a genuine semantic difference, not a coding error on my part — it reproduces through `syscall(SYS_fork)`. If it reaches a payload unconverted, the payload forks into two parents.

---

## SOURCES

Decisive programs, all in `/tmp/apex-b/`:

| file | what it proves |
|---|---|
| `rs.h`, `rs_syscall.s` | raw `svc` primitive, correct register discipline |
| `p_class.c` | class tags ignored; x16 = raw number; which values SIGSYS |
| `p_stubs2.c`, `mt2.c`, `mt3.c` | mach traps are `MOVN` (negative x16) and return in x0 without carry |
| `p_fork4.c`, `forkchk.c` | raw `fork` returns child pid in the child |
| `p_a.c` | raw `pipe` leaves `fd[1]` untouched |
| `p_zero.c`, `p_sig4.c`, `p_sig6.c` | raw `sigaction` installs but never delivers |
| `p_min.c`, `probesafe.c` | `mincore` is a safe probe; `0x80` = unmapped |
| `sub2.c`, `imgtest.c` | split-cache subcache headers + image-text table layout |
| `tbase.c`, `zr2.c` | export trie decode, `symbol = image_header + export_offset` |
| `zfinal.c` | **the zero-import resolver** (`nm -u` empty; matches `dlsym`) |
| `mydlsym.c` | the resolver as a drop-in `dlsym` replacement |
| `verify.c` → `dsym.txt` | ground-truth address table |
| `resolved.txt` | resolver output, side-by-side comparable with `dsym.txt` |
| `classify.c` | which libSystem functions are `svc` stubs vs real C |
| `raw_all.c` | per-syscall availability, one process each |
| `A`–`E`, `w2a`, `w4`, `hedge/` | the weak/strong robustness matrix |
| `zfinal`, `min`, `zf_nodc` | size measurements; `minos` sweep via `mv_*` |

Build commands are in `BUILD.md`. Machine-readable logs are in `logs/`.
