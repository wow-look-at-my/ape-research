# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Smallest-possible native loaders for Actually Portable Executables (APE) built by
[gosmopolitan](https://github.com/wow-look-at-my/gosmopolitan). One loader per platform,
each a single C file, usage `apeld PROG.com [args...]`. `README.txt` (markdown despite
the name) is the overview; `LOG.txt` is the chronological research log with size and
timing tables. `README.md` only points at those two.

## Build

```sh
./build.sh
```

Builds all four loaders into `bin/` with `zig cc` (CI pins zig 0.16.0). Tool names are
overridable by env var: `ZIG`, `LLD` (ld64.lld), `LLDLINK` (lld-link), `DLLTOOL`
(llvm-dlltool), `STRIP` (llvm-strip). CI runs it as
`LLD=ld64.lld-18 LLDLINK=lld-link-18 DLLTOOL=llvm-dlltool-18 STRIP=llvm-strip-18 ./build.sh`.

**The binaries in `bin/` are committed and must be byte-reproducible.** CI rebuilds them
and fails if any byte differs from the committed copy. Any change to a `*/apeld.c`,
`linux/apeld.ld`, `windows/kernel32.def`, or the flags in `build.sh` requires rebuilding
and committing the new `bin/` output in the same change. Size is a tracked metric: the
sizes table in `README.txt` and the step tables in `LOG.txt` record it.

Per-platform link pipelines (each chosen for size or reproducibility, see `LOG.txt`):
- Linux: freestanding, `-nostdlib`, raw syscalls, linker script `linux/apeld.ld` packs one
  PT_LOAD, then `llvm-strip --strip-sections` removes the section header table.
- macOS arm64: zig compiles the object, `ld64.lld -no_data_const` links against zig's
  bundled `libSystem.tbd` (zig's own Mach-O linker cannot merge `__DATA_CONST`, costing a
  16K page). The ad-hoc signature identifier is the output basename, so the output name
  must stay fixed.
- Windows: zig compiles, `lld-link /Brepro` links against an import lib generated from
  `windows/kernel32.def` (zig's link adds a non-reproducible PDB record). `/filealign`
  below 512 makes real Windows reject the PE even though wine runs it.

## Tests

Assertions live in `tests/<os>-<arch>.dats`, run by
[dats](https://github.com/wow-look-at-my/dats). dats output patterns are substrings, not
regexes. Tests run from the repo root and expect a probe APE at `out/probe.com`, built
from `testdata/probe` with the gosmopolitan Go toolchain:

```sh
(cd testdata/probe && GOOS=cosmo go build -o ../../out/probe.com .)
```

Running a suite (as CI does):

```sh
APELD=bin/apeld-linux-amd64 dats -v test tests/linux-amd64.dats
dats -v --no-sandbox test tests/windows-amd64.dats
```

The Linux suites read the loader path from `$APELD`; darwin and windows suites hardcode
`bin/...`. Each suite includes the stock shell/PE boot as a baseline. The probe prints
`args=`, `exe=`, `cwd=`, `env=` (from `APE_PROBE_ENV`), `goos=/goarch=`, and exits 3 when
its first arg is `fail`. Loader errors go to stderr prefixed `apeld: ` and exit 127.

On Linux CI, dats itself is an APE the stock shell cannot boot on 22.04 or arm64, so it
is booted through the loader under test. The real verification is the CI matrix in
`.github/workflows/ci.yml` (ubuntu 22.04/24.04 amd64+arm64, macOS 14/15/latest, Windows
2022/2025/latest); qemu-user cannot `execveat` a memfd, so the Linux arm64 loader has no
local test.

## APE layout facts every loader relies on

- Each payload is a complete ELF on a `0x10000` boundary, amd64 first. Loaders scan
  64K offsets for an ELF header with the host `e_machine`; no shell-script parsing.
- The payload's program headers already carry absolute file offsets into the APE; only
  its own `e_phoff` is payload-relative.
- Linux (`linux/apeld.c`): copy the whole file into a memfd, write the payload's 64-byte
  ELF header with `e_phoff += payload_offset` over offset 0, `execveat(AT_EMPTY_PATH)`.
  Custom `_start` stub, no libc. Cost: `os.Executable()` is `/memfd:NAME`.
- macOS arm64 (`darwin/apeld.c`): map PT_LOAD segments itself (text read into anonymous
  RW memory then `mprotect`ed, because W^X is enforced and an exec file mapping triggers
  whole-file hashing), refuse a load range holding live memory (`MAP_FIXED` would clobber
  it), build a Linux-shaped stack and auxv (deliberately no `AT_HWCAP`), then jump with
  `x3 = 8` (XNU), `x15 = &Syslib` (magic `"slib"`, version 10), `x16 = entry`. Syslib
  field order is an ABI with the gosmopolitan runtime and must not be reordered;
  syscall-shaped entries return `-errno`.
- Windows (`windows/apeld.c`): no CRT, imports only kernel32. `VirtualAlloc` the amd64
  payload at its fixed base `0x100000000`, resolve the import descriptor by hand (the
  runtime boots through `GetProcAddress`/`LoadLibraryA` IAT slots), set protections,
  rewrite the process command line in place so `os.Args` starts at the program, call
  the entry. Cost: `os.Executable()` is the loader's path.

CI also asserts linkage: Linux loaders have no INTERP or dynamic section, the PE imports
only `kernel32.dll`, the Mach-O loads only libSystem.

## research/

Scratch investigation of making the macOS loader survive hostile Apple changes. Contains
many committed probe binaries, `.o` files, and a vendored copy of Apple's dyld source
(`research/option-a/ds/`). Not built by `build.sh` or CI. The conclusions are in
`research/FINDINGS.txt` (with `option-a/REPORT.md`: content-based symbol discovery from
the dyld shared cache; `option-b/REPORT.md`: minimal dependency surface). Key conclusions:
raw `svc #0x80` works except class-0 syscalls; dyld requires the literal libSystem and
libdyld install names so rename immunity is disproven; `minos <= 15.3` allows zero dylib
load commands. A zero-import port exists as `research/probes/apeld2.c` but has not
replaced `darwin/apeld.c`.
