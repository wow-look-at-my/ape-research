# Research log

What was tried, what it measured, and what it changed. Newest entry last.

## Starting point

gosmopolitan APEs boot through a shell script at offset `0x800`. On Linux the script stages a copy under `/tmp`, writes an ELF header over the copy with `printf`, and execs it. On macOS arm64 it also extracts a gzipped `ape-m1.c` from offset `0x8000`. It compiles that with `cc`, caches the binary under `/tmp`, and execs it with the APE path. On Windows the file is a valid PE and nothing is extracted.

Research agents read the gosmopolitan linker, loader and runtime. The facts a native loader needs:

- Payloads sit on `0x10000` boundaries, amd64 first. Each is a complete ELF. Their program headers already hold absolute file offsets into the APE. Only the payload's own `e_phoff` is payload-relative.
- The amd64 payload detects the host from register `CL` at entry: `0` is Linux, `2` is Windows, `8` is XNU. Linux `execve` zeroes it.
- The arm64 payload on XNU expects `x3 = 8`, `x15` pointing at a `Syslib` table that starts with `"slib"`, and a Linux-shaped stack. The runtime accepts Syslib version `8` and up. The loader hands over version `10`.
- The runtime consumes `AT_PAGESZ`, `AT_RANDOM`, `AT_SECURE` and, on arm64, `AT_HWCAP`. Without `AT_HWCAP` on darwin it measures the CPU through sysctl, which is more honest than a hardcoded word.
- `os.Executable()` reads `/proc/self/exe` first and never falls back to `argv[0]` when that succeeds.

## Linux: memfd loader

Copy the file into a memfd, write the payload's ELF header with `e_phoff` rebased over offset 0, `execveat` the memfd. Freestanding C, raw syscalls. Works on the first try against the fizzbuzz APE.

Size steps, amd64:

| step | bytes |
|---|---|
| zig cc, static, gc-sections, stripped | 1384 |
| section header table removed | 985 |
| one PT_LOAD through a linker script | 816 |

The arm64 build has no local test. qemu-user cannot `fexecve` a memfd that holds an arm64 ELF. A plain hello world fails the same way. strace showed the right syscalls up to `execveat`. The arm64 GitHub runners then ran it for real. It boots the arm64 payload on ubuntu 22.04 and 24.04. The stock shell refuses those hosts, since a default gosmopolitan build does not claim linux/arm64.

Cost: `os.Executable()` reports `/memfd:PROG.com`.

## macOS: precompiled loader

A compact rewrite of what `ape-m1.c` does, compiled ahead of time with `zig cc -target aarch64-macos` against zig's bundled libSystem stubs. Differences from the embedded loader:

- Finds the payload on `0x10000` boundaries instead of decoding `printf` blobs in the shell script.
- Passes no `AT_HWCAP`, so the runtime asks sysctl.
- Uses libSystem's `pthread_jit_write_protect_np` directly instead of the comm-page APRR workaround.
- Keeps the live-memory scan over the load range, because `MAP_FIXED` at the `0x40000000000` link address replaces whatever is there.

| link | bytes | segments |
|---|---|---|
| zig's own Mach-O linker | 54528 | `__TEXT`, `__DATA_CONST`, `__DATA`, `__LINKEDIT` |
| `ld64.lld -no_data_const` | 36944 | `__TEXT`, `__DATA`, `__LINKEDIT` |

Code is under `0x2000` bytes. The rest is segment page padding at the arm64 page size. That layout plus dyld is the floor while libSystem is in the process. It has to be: the Syslib table is libSystem function pointers.

The ad-hoc "linker-signed" signature `ld64.lld` emits was enough. Every macOS runner in the matrix ran the loader.

The stock shell boot failed on the first macOS run with "ELF load range holds live memory". The probe APE had been built with the published toolchain `r1268`. Its embedded `ape-m1.c` still seeded the scan from `virtmin`, which is zero. The scan walked up from address zero and hit the loader's own text. The gosmopolitan tree already carries the fix. buildhost published it as `v505` minutes after that run.

## Windows: in-memory loader

The APE is already a valid PE. The OS maps the amd64 payload from the file through the PE header at `0x80`, read-only path or not, with no extraction. So Windows never had the problem this repo is about. The loader is still worth having as the in-memory route. It reads the file and places the payload at its fixed image base with `VirtualAlloc`. It resolves the import descriptor by hand, sets protections, and calls the PE entry. The loader shortens the process command line in place so `os.Args` starts at the program path.

Sizes and mistakes:

| step | bytes | note |
|---|---|---|
| zig cc link | 4608 | not reproducible: PE timestamp and a PDB record |
| `/Brepro` | 4608 | timestamp fixed, PDB record still differs between machines |
| lld-link, no PDB, `/Brepro` | 4096 | reproducible |
| `/filealign:16`, `.rdata` merged into `.text` | 3136 | wine runs it, Windows refuses it: "not a valid application" |
| `/filealign:512`, `.rdata` merged | 3584 | runs on every Windows runner in the matrix |

A tiny-PE variant with a `/align:512` section alignment was planned and dropped. Section alignment changes the virtual layout, not the file. The file was the same size.

Cost: `os.Executable()` reports the loader's path, because the payload is not a module the OS loaded.

## Reproducible binaries

The loaders are committed. CI rebuilds them and fails on a byte difference. The PE timestamp and PDB GUID broke that, fixed above. The Mach-O ad-hoc signature also did: its identifier is the output file's basename. The build always writes the same name. So it is stable.

## Test harness

The assertions started as shell steps in the workflow. That died on `bash -e` when a test expected exit `3`. They now live in `tests/*.dats`, one file per platform. dats patterns are substrings, not regular expressions.

dats itself ships as an APE. Ubuntu 22.04 cannot exec it (no binfmt fallback) and the arm64 runners refuse it through the shell. On Linux the workflow therefore boots dats through the memfd loader. The loader runs the test runner that tests the loader.

## Timings from the green run

Wall time of `probe.com hello`, output discarded. The shell columns are the first run, which stages the copy, and a second run with the copy present. On arm64 Linux the shell refuses the host at once.

| runner | shell, cold | shell, warm | loader |
|---|---|---|---|
| ubuntu-22.04 amd64 | 17 ms | 8 ms | 4 ms |
| ubuntu-24.04 amd64 | 11 ms | 5 ms | 3 ms |
| ubuntu-22.04-arm | refused | refused | 3 ms |
| ubuntu-24.04-arm | refused | refused | 3 ms |
| macos-14 | 14 ms | 12 ms | 2 ms |
| macos-15 | 15 ms | 14 ms | 4 ms |
| macos-latest | 31 ms | 28 ms | 5 ms |

On Windows nothing is staged, so the stock PE boot and the in-memory loader are one measurement each. The stock boot takes 23 to 26 ms. The loader takes 7 to 9 ms. It wins because it never maps the file as an image.

The macOS shell numbers are with the compiled `ape-m1` already cached under `/tmp`. The first run on a fresh machine also pays for `cc`, about a second on these runners.
