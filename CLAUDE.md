# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Smallest-possible native loaders for Actually Portable Executables (APE) built by [gosmopolitan](https://github.com/wow-look-at-my/gosmopolitan). There is one loader per platform, each a single C file. Usage is `apeld PROG.com [args...]`. `README.txt` is the overview. It is markdown despite the name. `LOG.txt` is the chronological research log with size and timing tables. `README.md` only points at those two.

## Build

```sh
./build.sh
```

`build.sh` builds every loader into `bin/` with `zig cc`. CI pins zig 0.16.0. Env vars override the tool names: `ZIG`, `LLD` (ld64.lld), `LLDLINK` (lld-link), `DLLTOOL` (llvm-dlltool), `STRIP` (llvm-strip). CI runs `LLD=ld64.lld-18 LLDLINK=lld-link-18 DLLTOOL=llvm-dlltool-18 STRIP=llvm-strip-18 ./build.sh`.

**The binaries in `bin/` are committed and must be byte-reproducible.** CI rebuilds them and fails if one byte differs from the committed copy. A change to `*/apeld.c`, `linux/apeld.ld`, `windows/kernel32.def` or the `build.sh` flags must commit the rebuilt `bin/` output too. Size is a tracked metric. The sizes table in `README.txt` and the step tables in `LOG.txt` record it.

Per-platform link pipelines (`LOG.txt` records why each was chosen):

- Linux: freestanding, `-nostdlib`, raw syscalls. The linker script `linux/apeld.ld` packs one PT_LOAD. Then `llvm-strip --strip-sections` removes the section header table.
- macOS arm64: zig compiles the object and `ld64.lld -no_data_const` links it against zig's bundled `libSystem.tbd`. zig's own Mach-O linker cannot merge `__DATA_CONST`. Each extra segment costs a 16K page. The ad-hoc signature identifier is the output basename. The output name must stay fixed.
- Windows: zig compiles and `lld-link /Brepro` links against an import lib made from `windows/kernel32.def`. zig's own link adds a non-reproducible PDB record. A `/filealign` below the 512-byte sector size makes Windows reject the PE. Wine still runs it.

## Tests

Assertions live in `tests/<os>-<arch>.dats`, run by [dats](https://github.com/wow-look-at-my/dats). dats output patterns are substrings, not regexes. Tests run from the repo root. They expect a probe APE at `out/probe.com`, built from `testdata/probe` with the gosmopolitan Go toolchain:

```sh
(cd testdata/probe && GOOS=cosmo go build -o ../../out/probe.com .)
```

Run a suite the same way CI does:

```sh
APELD=bin/apeld-linux-amd64 dats -v test tests/linux-amd64.dats
dats -v --no-sandbox test tests/windows-amd64.dats
```

The Linux suites read the loader path from `$APELD`. The darwin and windows suites hardcode `bin/...`. Each suite includes the stock shell or PE boot as a baseline. The probe prints `args=`, `exe=`, `cwd=`, `env=` (from `APE_PROBE_ENV`) and `goos=`/`goarch=`. It exits with status `3` when its first arg is `fail`. Loader errors go to stderr with the prefix `apeld: ` and exit 127.

On Linux CI, dats itself is an APE. The stock shell cannot boot it on 22.04 or arm64. CI therefore boots dats through the loader under test. The CI matrix in `.github/workflows/ci.yml` is the real verification. It covers ubuntu, macOS and Windows runner versions on amd64 and arm64. qemu-user cannot `execveat` a memfd. The Linux arm64 loader therefore has no local test.

## APE layout facts every loader relies on

- Each payload is a complete ELF on a `0x10000` boundary, amd64 first. Loaders scan 64K offsets for an ELF header with the host `e_machine`. They parse no shell script.
- The payload's program headers already carry absolute file offsets into the APE. Only its own `e_phoff` is payload-relative.
- Linux (`linux/apeld.c`) copies the whole file into a memfd. It writes the payload's 64-byte ELF header over offset 0, with `e_phoff` rebased by the payload offset. Then it calls `execveat` with `AT_EMPTY_PATH`. A custom `_start` stub replaces libc. Cost: `os.Executable()` returns `/memfd:NAME`.
- macOS arm64 (`darwin/apeld.c`) maps the PT_LOAD segments itself. Text goes into anonymous RW memory and then gets `mprotect`ed. W^X is enforced. An executable file mapping also triggers whole-file hashing. The loader refuses a load range that holds live memory. `MAP_FIXED` replaces live memory in silence. It builds a Linux-shaped stack and auxv with no `AT_HWCAP` on purpose. It jumps with `x3 = 8` (XNU), `x15 = &Syslib` and `x16 = entry`. The Syslib has magic `"slib"` and version 10. Its field order is an ABI with the gosmopolitan runtime. Do not reorder it. Syscall-shaped entries return `-errno`.
- Windows (`windows/apeld.c`) has no CRT and imports only kernel32. It places the amd64 payload at its fixed base `0x100000000` with `VirtualAlloc`. It resolves the import descriptor by hand. The runtime boots through the `GetProcAddress` and `LoadLibraryA` IAT slots. The loader sets protections and rewrites the process command line in place. Then `os.Args` starts at the program. Finally it calls the entry. Cost: `os.Executable()` returns the loader's path.

CI also asserts linkage. Linux loaders have no INTERP or dynamic section. The PE imports only `kernel32.dll`. The Mach-O loads only libSystem.

## Repository contents

The tree holds loader sources, build files, tests, docs and the macOS research record. Do not commit probes, scratch programs or build outputs other than `bin/`. External source goes in as a git submodule, never as a copy.

## research/

`research/FINDINGS.txt` holds the conclusions of the macOS hardening research. `research/option-a/REPORT.txt` covers content-based symbol discovery from the dyld shared cache. `research/option-b/REPORT.txt` covers a minimal dependency surface. `research/probes/apeld2.c` is the zero-import macOS loader port. `darwin/apeld.c` does not use it. The probe programs the reports cite are in git history at commit 1785223.

`research/dyld` is a git submodule of `apple-oss-distributions/dyld` at tag `dyld-1378`. Run `git submodule update --init` to fetch it. The dyld citations in the reports are paths inside it.
