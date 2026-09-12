#!/bin/sh
# Builds every loader into bin/ with zig cc. ZIG names the compiler
# (default: zig on PATH). Each build is static; CI asserts that.
set -eu
cd "$(dirname "$0")"
ZIG=${ZIG:-zig}
mkdir -p bin

COMMON="-Os -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall"

for t in x86_64:amd64 aarch64:arm64; do
	$ZIG cc -target "${t%%:*}-linux-musl" $COMMON -nostdlib -ffreestanding -static -fno-pic -fno-pie \
		-Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -s \
		-o "bin/apeld-linux-${t##*:}" linux/apeld.c
done

# zig compiles the object; ld64.lld links it, because zig's own Mach-O linker
# cannot merge __DATA_CONST into __DATA, and each segment costs a 16K page.
# libSystem.tbd comes from zig's bundled darwin libc stubs.
ZIGLIB=$($ZIG env 2>/dev/null | grep '"lib_dir"' | sed 's/.*: "\(.*\)".*/\1/')
[ -n "$ZIGLIB" ] || ZIGLIB=$(dirname "$(command -v "$ZIG")")/lib
$ZIG cc -target aarch64-macos $COMMON -c -o bin/apeld-darwin.o darwin/apeld.c
${LLD:-ld64.lld} -arch arm64 -platform_version macos 12.0 12.0 -L"$ZIGLIB/libc/darwin" -lSystem \
	-dead_strip -S -x -no_uuid -no_function_starts -no_data_const -fixup_chains \
	-o bin/apeld-darwin-arm64 bin/apeld-darwin.o
rm -f bin/apeld-darwin.o

# zig drops its bundled windows headers under -nostdlib, so compile and link apart.
$ZIG cc -target x86_64-windows-gnu $COMMON -c -o bin/apeld-windows.obj windows/apeld.c
# /Brepro derives the PE timestamp from the content, so the bytes are reproducible.
$ZIG cc -target x86_64-windows-gnu -nostdlib -Wl,--entry=start -Wl,--subsystem,console -Wl,--gc-sections -Wl,/Brepro \
	-o bin/apeld-windows-amd64.exe bin/apeld-windows.obj -lkernel32
rm -f bin/apeld-windows.obj bin/*.pdb

ls -l bin
