# ape-research

Smallest-possible native loaders for Actually Portable Executables (APE), one per platform. Each boots an APE without a shell, from memory where the OS allows it. License: MIT (see LICENSE).

The reference APEs come from [wow-look-at-my/gosmopolitan](https://github.com/wow-look-at-my/gosmopolitan). Its APEs boot through an embedded shell script today: it stages a copy under `/tmp`, writes a boot header over the copy, and execs it. On macOS arm64 the script also compiles a C loader with `cc` first.

## Which platforms need a loader at all

- Linux: the kernel cannot exec the file as it stands. The shell needs a writable, exec-capable `/tmp` for the staged copy. A read-only or `noexec` filesystem stops the program. The memfd loader removes that need. Registering the loader with `binfmt_misc` under the `F` flag removes the loader's own file too: the kernel holds it by descriptor, so an APE starts on a host with nothing writable and no loader file on it (see `LOG.txt`).
- macOS arm64: the shell needs `/tmp` and a working `cc` from Xcode. The precompiled loader removes both needs.
- Windows: nothing is extracted and nothing is modified. The file is a valid PE and the OS maps the payload straight from the file, read-only path or not. The Windows loader here is the in-memory equivalent, for the case where the file must not be started as an image.

## Loaders

| loader | source | how it boots the payload |
|---|---|---|
| `bin/apeld-linux-amd64`, `bin/apeld-linux-arm64` | `linux/apeld.c` | copies the APE into a memfd, writes the payload's ELF header over offset 0, `execveat`s the memfd |
| `bin/apeld-darwin-arm64` | `darwin/apeld.c` | maps the arm64 ELF payload itself, builds the SysV stack and auxv, hands over a Syslib table of libSystem entry points, jumps |
| `bin/apeld-windows-amd64.exe` | `windows/apeld.c` | reads the amd64 payload into its fixed image base, resolves the import table, calls the PE entry point |

Usage is the same everywhere:

```
apeld PROG.com [args...]
```

`build.sh` builds all four with `zig cc`. The binaries are committed so their size is tracked in history. CI rebuilds them and fails when the committed bytes differ.

## Sizes

| loader | bytes |
|---|---|
| apeld-linux-amd64 | 816 |
| apeld-linux-arm64 | 923 |
| apeld-darwin-arm64 | 36944 |
| apeld-windows-amd64.exe | 3584 |

## What the APE layout gives a native loader

- Each payload is a complete ELF at a 64 KiB boundary, amd64 first. Its program headers already carry absolute file offsets into the APE. Only `e_phoff` in the payload's own header is payload-relative.
- So a Linux loader needs no parsing of the shell script. It reads the payload header, adds the payload offset to `e_phoff`, writes those 64 bytes at offset 0, and execs. That is what the script's `printf` boot header does to the staged copy.
- The macOS payload expects `x3 = 8`, `x15` pointing at a `Syslib` whose first word is `"slib"`, and a Linux-shaped stack. The loader fills the Syslib v10 table from libSystem. Wrapped entries return `-errno`.
- The Windows PE header at offset 0x80 maps the amd64 payload at `0x100000000` with relocations stripped. The runtime resolves everything through the IAT slots for `GetProcAddress` and `LoadLibraryA`.

## Known costs of a memory launch

- Linux: `os.Executable()` returns `/memfd:PROG.com`. The runtime trims the ` (deleted)` suffix and never falls back to `argv[0]`. A program that re-execs itself by that path fails.
- Windows: `os.Executable()` returns the loader's path, since the payload is not a module the OS loaded. `os.Args` is correct: the loader shortens the process command line in place.
- macOS: none beyond the stock loader. `os.Executable()` resolves `argv[0]`.

## Testing

The assertions live in `tests/*.dats`, one file per platform, run by [dats](https://github.com/wow-look-at-my/dats). `.github/workflows/ci.yml` builds the loaders and a probe APE with the published gosmopolitan toolchain. It then runs each loader on a matrix of runner versions. The matrix covers ubuntu 22.04 and 24.04 on amd64 and arm64, macOS 14 and 15, and Windows 2022 and 2025. Every run also exercises the stock shell or PE boot as a baseline. It checks that the loaders import nothing beyond libSystem or kernel32. It prints timings.
