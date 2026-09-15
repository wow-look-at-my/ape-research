# Option A: content-based library discovery on macOS arm64

Author: empirical investigation, macOS 26.5 (Darwin 25.5.0), Apple M5, arm64.
Scratch: `/tmp/apex-a`. All artifacts buildable from the sources below.

---

## 0. Bottom line up front

**Option A works, and it is technically sound.** I built a resolver that finds
libSystem exports by *what they export*, with zero link-time symbol references
and zero dylib load commands in the probing binary, and it resolves all 53
Syslib-contract symbols (plus 184/187 in a broader sweep) to addresses that
**exactly match `dlsym`** (modulo two documented stub cases) and are
**provably callable**.

**But the rename scenario as posed is not survivable — not by Option A, not by
Option B, not by anything you can ship.** The reason is upstream of the loader:
dyld itself, which the kernel runs before your first instruction, hard-codes
`/usr/lib/system/libdyld.dylib` and `/usr/lib/libSystem.B.dylib` as literal
strings and **halts if either is missing** (`dyldMain.cpp:857`,
`dyldMain.cpp:897`). If Apple renames those, dyld aborts and *every* process on
the system dies — including one whose own dependencies are perfectly
content-discovered. Content discovery never gets a chance to run.

So Option A is real, but its value is **narrower and different** from the stated
thesis: it protects against *your* stale names and against *export relocation
inside the cache*, not against an Apple rename of libSystem, because dyld
already depends on that name.

The decisive new finding that changes the calculus: **the "zero dylib
reference" route is fully viable below minos 15.4**, which is a much simpler
mechanism than a full cache parser and achieves most of the same compatibility
goal. See §5 and §8.

---

## 1. What I verified working (with exact commands)

### 1.1 The shared-cache header layout — solved

The field offsets the prior attempt got wrong are:

| field | offset | type |
|---|---|---|
| `magic` | `0x00` | char[16] |
| `mappingOffset` | `0x10` | uint32 |
| `mappingCount` | `0x14` | uint32 |
| `imagesOffsetOld` | `0x18` | uint32 — **UNUSED, zero on modern caches** |
| `imagesCountOld` | `0x1C` | uint32 — **UNUSED, zero on modern caches** |
| `sharedRegionStart` | `0xE0` | uint64 |
| `sharedRegionSize` | `0xE8` | uint64 |
| `maxSlide` | `0xF0` | uint64 |
| `imagesTextOffset` | `0x88` | uint64 |
| `imagesTextCount` | `0x90` | uint64 |
| `subCacheArrayOffset` | `0x188` | uint32 |
| `subCacheArrayCount` | `0x18C` | uint32 |
| **`imagesOffset`** | **`0x1C0`** | uint32 |
| **`imagesCount`** | **`0x1C4`** | uint32 |
| `cacheSubType` | `0x1C8` | uint32 |

Confirmed three ways: (a) compiled Apple's own `dyld_cache_format.h` and printed
`offsetof`; (b) read the same values out of the on-disk cache; (c) used them to
successfully walk every image.

```
$ cc -o chdr chdr.c && ./chdr        # includes Apple's dyld_cache_format.h
imagesOffset=0x1c0
imagesCount=0x1c4
...
$ ./cacheprobe
magic='dyld_v1  arm64e'
imagesOffsetOld=0x0 imagesCountOld=0
sharedRegionStart=0x180000000 size=0x165114000 maxSlide=0x10000000
imagesOffset=0x298 imagesCount=3646
  img[0] addr=0x18008c000 pathOff=0x394c8 path='/usr/lib/libobjc.A.dylib'
```

The prior attempt's `0x18/0x1C` returned 0 because those are deliberately
retired ("moved to imagesOffset to prevent older dsc_extractors from
crashing"); `0xE0/0xE4` is `sharedRegionStart`, which is a valid-looking 64-bit
value whose low half is garbage — that explains "implausible values then
segfault."

### 1.2 The address-translation rules — solved

```
slide        = cacheBase - header.sharedRegionStart
image_hdr    = (uint8_t*)(images[i].address + slide)      // live mach_header_64
linkeditLive = (uint8_t*)(linkeditSeg.vmaddr + slide)
trie         = linkeditLive + (trie_dataoff - linkeditSeg.fileoff)
```

The last line is the one that defeated the prior attempt. `dataoff` in
`LC_DYLD_EXPORTS_TRIE` / `LC_DYLD_INFO_ONLY` is a **file offset within the
subcache that holds `__LINKEDIT`**, not a cache-relative or image-relative
offset. On this machine `__LINKEDIT.fileoff == 0x4000` and the trie lives in
`.04.dyldlinkedit` / `.08.dyldlinkedit` etc. Subtracting `linkeditSeg.fileoff`
is mandatory; adding is wrong; treating it as absolute is wrong.

`cacheBase` itself comes from:

```c
uint64_t base; syscall(294 /* shared_region_check_np */, &base);
```

In a live process the *entire* shared region (all 12 subcaches, ~5.7 GiB of VM)
is mapped contiguously starting at `cacheBase`, so `cacheBase + subCacheVMOffset`
addresses each subcache. That is why `images[i].address + slide` works with a
single global slide.

### 1.3 The export trie parser — verified against ground truth

Node layout (correct): `[terminalSize ULEB][terminal payload if size>0]
[childCount ULEB][child: NUL-terminated edge + ULEB childOffset]`.
Terminal payload begins with a `flags` ULEB, then:

| flags | meaning | payload after flags |
|---|---|---|
| `0x00` | regular | ULEB vmOffset |
| `0x02` | `KIND_ABSOLUTE` | ULEB absolute address |
| `0x04` | `KIND_THREAD_LOCAL` | ULEB vmOffset |
| `0x08` | `REEXPORT` | ULEB ordinal + NUL import name |
| `0x10` | `STUB_AND_RESOLVER` | ULEB *implementation* + ULEB *stub* |

Two traps I hit and fixed:

1. **Symbol names in the trie carry the leading underscore.** Look up
   `_pthread_create`, not `pthread_create`. (Exception: re-export *import* names
   sometimes carry two, e.g. `__platform_strcmp`.)
2. **`0x10` is `STUB_AND_RESOLVER`, not "absolute."** My first version conflated
   it with `0x02`, which produced a wrong address for `strcmp`/`strncmp`. I
   measured the correct interpretation empirically (§3.2): the **first** ULEB is
   the callable implementation; the **second** is an arm64e dispatch stub that
   returns garbage when called with real arguments.

Validation: parse a file on disk, dump its trie, compare to `nm`.

```
$ ./triedump libtlib.dylib
EXPORT   _my_export_one                           = 0x2d0 flags=0x0
EXPORT   _my_export_two                           = 0x2d8 flags=0x0
(2 symbols)
$ nm -gU libtlib.dylib
00000000000002d0 T _my_export_one
00000000000002d8 T _my_export_two
```

Coverage across the whole live cache — this is the strongest robustness number:

```
$ ./cov
images=3646 parsed-ok=3646 no-trie=0 bad=0 total-symbols=3641939
```

**Every image in the live cache parses, 3.64 million symbols enumerated, zero
malformed.**

### 1.4 End-to-end resolution vs `dlsym`

```
$ ./scaletest
total=187 MATCH=184 DIFF=2 NOTFOUND=1 (dlsym==0: 0)
DIFF      strcmp   cache=0x188d2f90c dlsym=0x188d2c730 (/usr/lib/system/libsystem_c.dylib)
DIFF      strncmp  cache=0x188d2f918 dlsym=0x188d2c9b0 (/usr/lib/system/libsystem_c.dylib)
NOTFOUND  dlinfo
```

- The 2 `DIFF`s are `STUB_AND_RESOLVER` exports (correctly identified; see §3.2).
- `dlinfo` is **genuinely absent** from this OS's cache (`nm`/`dyld_info` confirm).
  A content resolver correctly reports "not found" rather than inventing an
  address — the desired failure mode.
- Re-exports are followed: `memcpy` (a re-export in `libsystem_c`) resolves
  through to `libsystem_platform`.

### 1.5 The pointers are callable, not just equal

```
$ ./calltest
strcmp via content ptr = -1, via libc = -1  OK
strncmp via content ptr = 0, via libc = 0  OK
malloc/free via content ptrs: OK (block 0x104a71a00)
getpid via content ptr = 81028, libc = 81028  OK
pthread_self via content ptr = 0x1f4ea1e80, libc = 0x1f4ea1e80  OK
```

### 1.6 The real deliverable: a full Syslib table built by content

`syslibdemo.c` is a **zero-dylib-load-command, zero-undefined-symbol** Mach-O
that builds the entire 53-entry Syslib contract from cache contents:

```
$ nm -u t/syslibdemo      # (empty)
$ ./munge t/syslibdemo show | grep -E 'DYLIB|DYLINKER'
[ 8] LC_LOAD_DYLINKER   name='/usr/lib/dyld'
(no LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB)
$ ./t/syslibdemo
building Syslib from content: 3646  cache images
  [0 ] fork -> 0x0000000188bd31a0  /usr/lib/system/libsystem_c.dylib
  ...
  [52 ] sysctlnametomib -> 0x0000000188be0bc4  /usr/lib/system/libsystem_c.dylib
resolved 53 /53  Syslib entries
write() via content-discovered table works
getentropy via table rc=0  bytes=0x674344ad05fa8b12
fork() via table = 0  (0 = child)
```

Note where the symbols actually live — **not one comes from
`/usr/lib/libSystem.B.dylib`**:

| image supplying Syslib symbols | count |
|---|---|
| `libsystem_kernel.dylib` | 18 |
| `libsystem_pthread.dylib` | 15 |
| `libsystem_c.dylib` | 10 |
| `libdispatch.dylib` | 5 |
| `libdyld.dylib` | 4 |
| `libsystem_platform.dylib` | 1 |

`libSystem.B.dylib` is a pure re-export umbrella: it exports only 3 symbols of
its own and re-exports 39 sub-libraries. This matters for §4.

### 1.7 Measured sizes

| artifact | bytes | notes |
|---|---|---|
| `t/min_weak` | 16,792 | smallest working weak-linked binary |
| `t/nd3` | 34,832 | minos 11, **zero** dylib load commands, runs |
| `t/apexdemo_nolib` | 51,328 | content discovery, zero undefined, zero dylib cmds |
| `t/syslibdemo` | 51,408 | full 53-entry Syslib by content, zero undefined |
| `darwin/apeld.c` (Option B, given) | ~36,944 | links libSystem, 67 undefined symbols |

Content discovery costs **~14.5 KiB** of code (51,408 − 36,944) versus the
conventional loader, and removes all 67 link-time symbol dependencies.

---

## 2. The irreducible dependency list

These are real, measured, and I could not defeat them.

| # | dependency | enforced by | evidence |
|---|---|---|---|
| 1 | `LC_LOAD_DYLINKER` == exactly `/usr/lib/dyld` | XNU kernel | rename → SIGKILL (rc=137) before any user instruction |
| 2 | dyld must find `/usr/lib/system/libdyld.dylib` | dyld, literal string | `dyldMain.cpp:857` `halt("libdyld.dylib not found")` |
| 3 | dyld must find `/usr/lib/libSystem.B.dylib` | dyld, literal string | `dyldMain.cpp:897` `halt("program does not link with libSystem.B.dylib")`; set in `DyldRuntimeState.cpp:469` by exact `strcmp` |
| 4 | at least one dylib edge that resolves, **if minos ≥ 15.4** | dyld `Policy::enforceHasLinkedDylibs()` | epoch gate `>= spring2025`; measured boundary exactly between 15.3 (runs) and 15.4 (aborts) |
| 5 | class-0 raw syscalls are fatal (x16 small) | kernel | prior finding, reproduced |
| 6 | `filetype` must be `MH_EXECUTE` (2) | kernel | every other filetype → `Exec format error` (126); `MH_DYLIB`/`MH_BUNDLE` are not launchable |
| 7 | `MH_DYLDLINK` flag must be set on `MH_EXECUTE` | kernel | clearing it → SIGKILL (137) |

### The one that kills the thesis

**Items 2 and 3 are name-based and live in dyld, before your code runs.** I
verified this in the source rather than guessing:

```
research/dyld/dyld/DyldRuntimeState.cpp:467-470
    if ( strcmp(installName, "/usr/lib/system/libdyld.dylib") == 0 )
        setDyldLoader(ldr);
    else if ( strcmp(installName, "/usr/lib/libSystem.B.dylib") == 0 )
        libSystemLoader = ldr;
```

`libdyldLoader` and `libSystemLoader` can *only* be set by those exact strings.
`dyldMain` then halts unconditionally if either is null. There is no content
fallback, no inode check, no UUID check — a literal `strcmp`.

I could not construct any arm64 launchable Mach-O that avoids this: every
`MH_EXECUTE` goes through `prepare()` → `loadDependents()` → `state.add()` →
this `strcmp`.

### The subtle one: even zero dylib commands does not escape the name

This is the sharpest adversarial finding. dyld, for a binary with **no** dylib
load commands, *synthesizes* a libSystem dependency — but it synthesizes it **as
the literal string**:

```
research/dyld/mach_o/UnsafeHeader.cpp:1403-1422
    if ( (count == 0) && !stopped ) {
        // The dylibs that make up libSystem can link with nothing
        ...
        callback("/usr/lib/libSystem.B.dylib", LinkedDylibAttributes::regular,
                 Version32(1,0), Version32(1,0), true, stopped);
    }
```

So the zero-dylib trick (§5) is *also* name-dependent through dyld. It buys you
survival of *dyld's epoch validation*, not survival of a rename.

---

## 3. What failed, and why

### 3.1 Everything I tried to escape dependency #2/#3 — all failed

| attempt | result |
|---|---|
| `filetype` 1,3,4,6..12 (`MH_OBJECT`, `MH_DYLIB`, `MH_BUNDLE`, …) | `Exec format error` (126) |
| clear `MH_DYLDLINK` on `MH_EXECUTE` | SIGKILL (137) |
| dylinker name off by one char / symlink / other valid dyld | SIGKILL (137) |
| static (no dylinker) | SIGKILL (137) |
| drop all dylib load commands, minos 26 | `missing LC_LOAD_DYLIB` abort (134) |
| drop all dylib load commands, minos 11 | **runs** (but dyld synthesizes libSystem.B by name) |
| only weak reference, renamed away | `libdyld.dylib not found` abort (134) |
| `LC_UNIXTHREAD` instead of `LC_MAIN` | prior finding: no proper argc/argv; not re-tested |
| x3 "apple vector" as a libSystem pointer table | **false** — see §3.3 |

### 3.2 The `STUB_AND_RESOLVER` measurement

Prior notes called flag `0x10` "absolute"; it is not. Measured on
`__platform_strcmp` in `libsystem_platform.dylib`
(first ULEB → 0x790C, second → 0x920):

```
$ ./stubfinal
__platform_strcmp   r1=0x188d2f90c r2=0x188d28920
   CALL r1: strcmp('abc','abd')=-1 (want -1)
__platform_strncmp  r1=0x188d2f918 r2=0x188d289b4
   CALL r1: strncmp('abcx','abcy',3)=0 (want 0)
```

Calling `r2` as `strcmp` returned `-1999452368` (garbage). Calling `r1` returned
`-1`. So: **first ULEB = callable implementation, second = arm64e dispatch stub.**
`dlsym` additionally resolves to a CPU-optimised variant, hence the benign
address difference. This is the correct, if slightly uncomfortable, behaviour.

### 3.3 The x3 "apple vector" is dyld launch parameters, not a libSystem table

The prior note hoped x3 was "an array of char* pointers (12 entries, then
string data)". It **is** an array of strings — but they are `key=value` launch
parameters, not pointers to libSystem:

```
$ ./t/vec2
saved_x3 = 0x000000016f842680
  str[0 ] = 'executable_path=./t/vec2...'
  str[4 ] = 'ptr_munge=main_stack='
  str[6 ] = 'executable_file=0x1a01000010,0x150c636dyld_file...'
  str[8 ] = 'executable_cdhash=605d5f82e955c55270fc7b91dca48a...'
  str[9 ] = 'executable_boothash=6982bda0373f93d83d9ee7ce8c6f...'
```

Confirmed in source: consumed by `ProcessConfig::Process::appleParam()` via
`_simple_getenv((const char**)apple, key)` (`DyldProcessConfig.cpp:572`).
**There is no pointer table to libSystem anywhere in the process image.** A
loader must resolve everything itself. (One nice side effect: `executable_path=`
is available here, which is useful for `AT_EXECFN`.)

Also corrected: x3 *is* valid at entry. My first C probe read zero because the
prologue clobbered x3 before saving it; an assembly shim (`x3shim.S`) captures
the real value. So the earlier "x3 is garbage" conclusion was a measurement
artifact, not a kernel behaviour.

### 3.4 The prior attempt's segfaults, explained

All three causes are now identified:
1. Following `imagesOffset` at `0x18` → 0 → dereference near-null.
2. Following `0xE0` as `imagesOffset` → `sharedRegionStart` low half
   (`0x80000000`) → wild pointer.
3. Placing the trie at `linkeditLive + dataoff` (without subtracting
   `linkedit.fileoff = 0x4000`) → valid-looking bytes that are not a trie →
   unbounded child walk → segfault.

---

## 4. Does content discovery genuinely survive a rename?

**Yes for the dependency your binary declares. No for the rename that matters,
because dyld dies first.** Rigorously:

### 4.1 Proven: the resolver never reads a path

`cacheresolve.c` / `syslibdemo.c` / `apexdemo.c` never call `open`, `stat`, or
`dlopen` to find the library. They iterate `images[i]` and match **only** on
trie contents. The path string is used solely for *reporting*. Simulating
renames by hiding images whose paths match:

```
$ ./renamesim
hide='(nothing)   ': resolved 8/8
hide='libSystem.B ': resolved 8/8     <- pretend libSystem.B is renamed away
hide='libSystem   ': resolved 8/8     <- pretend the whole family is renamed
hide='libdyld     ': resolved 7/8     <- only dlopen/dlsym are lost
hide='lib' (all system libs): resolved 0/8 (clean failure, no crash)
```

So if a *future dyld* stops name-matching and loads a renamed library, content
discovery would keep working — for 7 of 8 symbols even with the entire
`libSystem*` family renamed, because the exports physically live in
`libsystem_kernel`, `libsystem_pthread`, etc.

### 4.2 The catch that makes it moot

For the rename to be survivable *at all*, dyld must first get past
`halt("libSystem.B.dylib not found")`. It cannot, by construction (§2). So:

- **Today's dyld, libSystem renamed** → dyld aborts → Option A and Option B both
  die identically. Content discovery is dead code.
- **A future dyld that stopped name-matching** → Option A survives; Option B
  (which weak-links `libSystem.B.dylib`) would have its reference fail and, on
  minos ≥ 15.4 with no surviving edge, abort.

In other words, Option A only pays off in the world where Apple *also* fixes
dyld's own brittleness — which is the world where the attack is already
defanged.

### 4.3 Removal / splitting / export movement

- **Removal of `libSystem.B.dylib`:** fatal (dyld hard-codes it), same as rename.
- **Splitting/relocating exports between images:** this is where Option A
  genuinely wins, and it is not hypothetical — `libSystem.B.dylib` already
  exports only 3 symbols and re-exports 39 sub-libraries, and every one of the
  53 Syslib symbols comes from a sub-library, not from libSystem.B itself. A
  loader that hard-links `libSystem.B` and relies on its flat namespace would be
  the fragile one. Content discovery is indifferent to which image holds an
  export.
- **A symbol moving image:** resolved correctly, because resolution is a scan
  over all images, not a lookup in one.

### 4.4 What the mechanism itself depends on that Apple controls

| dependency | risk | mitigation |
|---|---|---|
| `shared_region_check_np` = syscall 294 | Apple could remove it | **fallback found and verified**: scan up from `sp` for a 16K-aligned pointer that validates as a cache header. Stable at `sp+656` across 5 runs; agrees with 294. |
| header offset `0x1C0`/`0x1C4` | a version bump could move it | field validation: magic `dyld_v1`, `0x200 ≤ imagesOffset ≤ 0x10000`, `0 < imagesCount ≤ 100000`, table fits in the mapping. Verified to reject junk (`0xdeadbeef`, `0xffffffff`, `0`, bad magic) without crashing. Could still be extended to a version-keyed offset table. |
| trie format | stable since 10.6; `LC_DYLD_EXPORTS_TRIE` vs `LC_DYLD_INFO_ONLY` both handled | — |
| *the library's own name* | **not used** | this is the point of Option A |

So the *mechanism* is defensible. It is only the *scenario* that is unwinnable.

---

## 5. Decisive and surprising: the minos-15.4 gate

This is a genuinely new compatibility lever, and it is cheaper than content
discovery.

`Policy::enforceHasLinkedDylibs()` returns true only when the **binary's own**
enforcement epoch is `>= spring2025`. The epoch is derived from the binary's
`minos`, i.e. it is a property of *our* file and cannot be changed under us.

```
$ for v in 11.0 ... 26.0; do  # build -weak-lSystem, then drop all dylib cmds
minos=11.0  no-dylib-refs rc=3
minos=12.0  rc=3
minos=13.0  rc=3
minos=14.0  rc=3
minos=15.0  rc=3
minos=15.1  rc=3
minos=15.2  rc=3
minos=15.3  rc=3
minos=15.4  rc=134  dyld: missing LC_LOAD_DYLIB ...
minos=16.0  rc=134
minos=26.0  rc=134
```

Boundary is exactly **15.4**. A binary declaring `minos 11.0` with **zero dylib
load commands** runs, because dyld's `forEachLinkedDylib` synthesizes the
libSystem edge for it (§2) and the epoch check is grandfathered.

Consequences:

- A minos-11 binary carries **no dylib install-name string at all** in its load
  commands. Rename `libSystem.B.dylib` and *our* file still refers to nothing
  by that name — though dyld's internal synthesis still uses it (§2).
- This gets you the same "no name in my binary" property as a hand-built
  `LC_LOAD_WEAK_DYLIB` hack, with **no byte patching and no re-signing**.

**Important caveat, verified:** minos is a floor, not a ceiling. The binary still
runs on macOS 26.5. But whether dyld *chooses* to enforce the spring-2025 policy
from the binary's minos is itself dyld behaviour Apple can change in a future OS;
today it is grandfathered.

---

## 6. Redundancy beats cleverness: the cheapest real defence

Measured, not theorised. Two weak references, one of which will survive any
single rename:

```
# binary links WEAK libSystem.B + WEAK libz.1
$ ./munge t/two_weak show | grep 'name='
[11] LC_LOAD_WEAK_DYLIB  name='/usr/lib/libSystem.B.dylib'
[12] LC_LOAD_WEAK_DYLIB  name='/usr/lib/libz.1.dylib'

$ ./t/two_weak a b c                # baseline
rc=4
$ ./munge t/two_weak_renamed name /usr/lib/libSystem.B.dylib /usr/lib/libMacos.B.dylib
$ ./t/two_weak_renamed a b c        # libSystem renamed away
rc=4                                # STILL RUNS
```

versus renaming the *only* weak reference:

```
$ ./t/renamed_weak a b c
dyld: libdyld.dylib not found       # abort
rc=134
```

And on minos 26 (post-epoch), with `libSystem.B` renamed but `libz` intact:

```
$ ./t/m26_two a b c
rc=4                                # runs: one surviving weak edge satisfies policy
```

**A second, unrelated `LC_LOAD_WEAK_DYLIB` costs one load command (~48 bytes)
and converts a hard abort into a successful launch** for any single-library
rename. That is a far better return than 14.5 KiB of cache parser, and it stacks
with Option A. Note that both-renamed still aborts (`libdyld.dylib not found`),
so this is redundancy, not immunity.

---

## 7. Why `libdyld.dylib` matters to the *loader* specifically

Worth stating explicitly for your design: `apeld.c` and `ape-m1.c` both call
`dlopen`/`dlsym` to fill 4 of 53 Syslib slots. Those live in `libdyld.dylib`.
Content discovery resolves them from the same cache walk, so a loader built on
Option A needs **no** `dlopen`/`dlsym` at all — removing the last runtime
dependency on dyld's public API. That is a small but genuine structural win:
the loader becomes a pure reader of already-mapped memory plus raw syscalls.

The trade: your loader will then hold raw pointers it cannot re-resolve after an
OS update, where `dlsym` would re-resolve. Given the loader is short-lived
(it maps the payload and jumps), that is acceptable.

---

## 8. Blunt recommendation

**Option A is worth building, but not for the reason stated, and not as the
first thing you build.**

Concretely:

1. **Ship Option B first** (weak-link + raw syscalls), plus the two cheap
   hardening measures below. It is a day of work and covers more real-world
   breakage than the cache parser does.
2. **Add a second weak dylib reference** (§6). ~48 bytes, converts a hard abort
   into a launch under any single rename. Highest ratio in this report.
3. **Set `minos 11.0`** so dyld's spring-2025 `enforceHasLinkedDylibs` stays
   grandfathered (§5). Keep at least one weak edge anyway so you are not relying
   on the grandfather clause alone.
4. **Then add content discovery as the Syslib source** (§1.6, 53/53, ~2 KiB of
   the 14.5 KiB is the resolver; the rest is I/O scratch). It is robust
   (3646/3646 images, 3.64M symbols, zero failures), it matches `dlsym` exactly,
   and it eliminates all 67 link-time symbols in `apeld.c`. Its *real* payoff is
   immunity to internal cache reorganisation and to `libSystem.B` being a
   re-export umbrella — which it already is.
5. **Do not claim rename immunity.** It is false (§2, §4.2). If you brief this
   as "survives libSystem being renamed," you will be wrong in the one scenario
   where it matters, because dyld halts at
   `dyld_main → prepare() → state.add()` before your loader is entered.
6. **Do keep the fallback path** for `shared_region_check_np` (§4.4). It is
   verified working and costs ~20 lines.

### Honest risk ledger

| claim | status |
|---|---|
| header offsets `0x1C0`/`0x1C4` | **proven** (3 independent ways) |
| trie placement formula | **proven** (3646/3646 images parse, 3.64M symbols) |
| resolver matches `dlsym` | **proven** for 184/187; 2 explained; 1 genuinely absent |
| resolved pointers are callable | **proven** for 6 functions incl. malloc/free, fork |
| full 53-entry Syslib by content | **proven**, zero undefined symbols |
| content discovery ignores paths | **proven** by construction + renamesim |
| survives a libSystem rename | **disproven** — dyld halts first |
| zero-dylib binary runs on minos < 15.4 | **proven**, boundary measured at 15.4 |
| zero-dylib binary escapes the name dependency | **disproven** — dyld synthesizes the literal path |
| second weak ref rescues a single rename | **proven** |
| cache base without syscall 294 | **proven** via stack scan fallback |
| parser is robust to corrupt header fields | **proven** (rejects junk, no crash) |
| behaviour under a *future* cache format change | **assumed** — field validation only; no forward compatibility guarantee |

---

## 9. Full source of the decisive programs

All under `/tmp/apex-a`.

### 9.1 `cacheresolve.c` — the resolver (verified 184/187)

Key excerpt (full file on disk, ~430 lines). Header constants, the trie walker,
and the content scan:

```c
#define CH_SR_START     0xE0
#define CH_IMAGES_OFF   0x1C0
#define CH_IMAGES_CNT   0x1C4

// Node: [terminalSize ULEB][terminal payload][childCount ULEB][edge+offset]*
TrieHit trieLookup(const uint8_t *trie, uint64_t size, const char *name /* _-prefixed */) {
    const uint8_t *end = trie + size, *node = trie; const char *s = name;
    for (int depth = 0; depth < 256; depth++) {
        int ok = 1; const uint8_t *q = node;
        uint64_t termSize = uleb(&q, end, &ok);
        if (!ok || q > end || termSize > (uint64_t)(end - q)) return NOTFOUND;
        const uint8_t *term = q; q += termSize;
        if (*s == 0 && termSize) {
            const uint8_t *t = term;
            uint64_t flags = uleb(&t, term + termSize, &ok);
            if (flags & 0x08) { /* REEXPORT: ordinal + optional import name */ }
            if (flags & 0x10) { /* STUB_AND_RESOLVER: impl, then stub */ }
            if (flags & 0x02) { /* KIND_ABSOLUTE */ }
            if (flags & 0x04) { /* KIND_THREAD_LOCAL */ }
            return ADDRESS(uleb(&t, term + termSize, &ok));
        }
        uint64_t childCount = uleb(&q, end, &ok);
        /* ...match edge bytes, follow childOffset... */
    }
}

// Live address of the trie for an image.
const uint8_t *linkLive = (const uint8_t *)(im->linkVm + g_slide);
im->trie = linkLive + (int64_t)im->expOff - (int64_t)im->linkFileOff;

// Content scan: no path is ever consulted.
for (uint32_t i = 0; i < g_nImgs; i++) {
    const uint8_t *mh = (const uint8_t *)(g_imgs[i].address + g_slide);
    if (*(const uint32_t *)mh != 0xfeedfacf) continue;
    Image t = {0}; t.base = g_imgs[i].address; t.mh = mh; findExportInfo(&t);
    uint64_t a = lookupIn(&t, symPlain, 0, 0);   /* follows re-exports */
    if (a) return a;
}
```

### 9.2 `syslibdemo.c` — 53/53 Syslib from content, zero undefined

Standalone, `#include`-free, raw-syscall only. Full source on disk (~250 lines).
It contains its own `ImgX` per-image cache, the same trie walker, and the exact
`SYSLIB[]` order from `ape-m1.c` / `os_cosmo_arm64.go`:

```c
#define CH_SR_START 0xE0
#define CH_IMAGES_OFF 0x1C0
#define CH_IMAGES_CNT 0x1C4

static void loadImage(u32 i) {
    u32 ioOff = *(const u32*)((const u8*)g_cache + CH_IMAGES_OFF);
    const ImgInfo *ii = (const ImgInfo*)((const u8*)g_cache + ioOff);
    g_ix[i].base = ii[i].address + g_slide;
    /* walk load commands; pick up __LINKEDIT vmaddr/fileoff and
       LC_DYLD_EXPORTS_TRIE (0x80000033) or LC_DYLD_INFO_ONLY (0x80000022) */
    g_ix[i].trie = (const u8*)(g_ix[i].lv + g_slide)
                 + (i64)g_ix[i].eo - (i64)g_ix[i].lf;
}
```

Build and run (exact commands, reproducible):

```bash
SDK=$(xcrun --show-sdk-path)
clang -c -arch arm64 -O2 -fno-stack-protector -fno-builtin -o syslibdemo.o syslibdemo.c
ld -arch arm64 -e _main -o t/syslibdemo syslibdemo.o \
   -weak-lSystem -syslibroot "$SDK" -platform_version macos 11.0 11.0
./munge t/syslibdemo dropdylibs      # remove the LC_LOAD_WEAK_DYLIB
codesign -f -s - t/syslibdemo        # re-sign after byte edits
./t/syslibdemo
```

### 9.3 `munge.c` — Mach-O load-command editor

Ops: `show`, `name <old> <new>`, `filetype <n>`, `hdrflags <n>`, `dropdylibs`.
This is what makes the rename experiments possible. Always `codesign -f -s -`
after editing.

### 9.4 `basefallback.c` — cache base without syscall 294

```c
u64 hi = sp + 4*1024*1024;
for (u64 a = sp & ~7UL; a < hi; a += 8) {
    u64 v = *(const u64 *)a;
    if (v >= 0x180000000UL && v < 0x1A0000000UL && (v & 0x3FFF) == 0)
        if (hdrValid(v)) { found = v; break; }   // magic + plausible table fields
}
```

### 9.5 Other artifacts on disk

| file | purpose |
|---|---|
| `cacheprobe.c` / `cacheprobe2.c` | offline header + image-table dump |
| `liveprobe.c` | in-process header, slide, load-command dump |
| `subs.c` | enumerate all 12 subcaches and their mappings |
| `cov.c` | parse every image's trie (3646/3646) |
| `triedump.c` | offline trie dump, verified vs `nm` |
| `scaletest.c` | 187-symbol sweep vs `dlsym` |
| `calltest.c` | call resolved pointers |
| `stubfinal.c` | prove first/second ULEB semantics for `0x10` |
| `flagsdbg.c` / `rq.c` | raw terminal flag/byte dumps |
| `renamesim.c` | rename simulation by hiding images |
| `valid.c` | header validation rejects junk |
| `x3shim.S` / `vec2.c` | capture and interpret x3 at entry |
| `dup.c` / `lc2.c` / `one.c` | image lookup and load-command enumeration |

---

## 11. Reproducible confidence sweep

Run from `/tmp/apex-a` after building the tools (§1). Exit code equals `argc`
when a program runs to completion.

```
1. header layout (3 ways):        imagesOffset=0x1c0 confirmed
2. trie coverage:                 parsed-ok=3646 (of 3646), 3,641,939 symbols
3. Syslib by content:             resolved 53 /53
4. zero-dylib demo:               0 LC_LOAD*DYLIB commands, runs
5. rename sim, libSystem hidden:  resolved 8/8
6. minos sweep:                   15.3 -> rc=4 (runs); 15.4 -> rc=134 (abort)
7. second weak ref:               baseline rc=4; libSystem renamed rc=4 (survives)
   single weak ref:               renamed rc=134 (aborts)
8. cache-base fallback:           fallback == syscall 294 result
```

All eight verified in the final sweep. Artifacts and sources are preserved in
`/tmp/apex-a`, and every tool rebuilds cleanly from its `.c` file.


Content-based discovery is **not theater** — it is a correct, robust, fully
verifiable mechanism that resolves the entire Syslib surface to callable
addresses without a single link-time symbol, and it is the *only* approach
indifferent to which image holds an export (which already matters, since
`libSystem.B.dylib` exports just 3 symbols and sub-libraries hold all 53 Syblib
entries). But it **does not save you from the stated scenario**, because the
scenario kills dyld before your loader is reached: dyld hard-codes
`/usr/lib/system/libdyld.dylib` and `/usr/lib/libSystem.B.dylib` with `strcmp`
and `halt()`s when they are gone, and the kernel hard-codes `/usr/lib/dyld` with
a SIGKILL. Build Option A for the resilience it genuinely provides —
reorganisation immunity, zero symbol dependencies, no `dlopen` — but harden the
*launch* path with the cheap, proven measures: a second weak dylib reference and
`minos 11.0`. Those, not the cache parser, are what will keep you running.
