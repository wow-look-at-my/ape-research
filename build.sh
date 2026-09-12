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

$ZIG cc -target aarch64-macos $COMMON -Wl,-dead_strip -Wl,-S -Wl,-x \
	-o bin/apeld-darwin-arm64 darwin/apeld.c

# zig drops its bundled windows headers under -nostdlib, so compile and link apart.
$ZIG cc -target x86_64-windows-gnu $COMMON -c -o bin/apeld-windows.obj windows/apeld.c
# /Brepro derives the PE timestamp from the content, so the bytes are reproducible.
$ZIG cc -target x86_64-windows-gnu -nostdlib -Wl,--entry=start -Wl,--subsystem,console -Wl,--gc-sections -Wl,/Brepro \
	-o bin/apeld-windows-amd64.exe bin/apeld-windows.obj -lkernel32
rm -f bin/apeld-windows.obj bin/*.pdb

ls -l bin
