#!/bin/sh
# Builds every loader into bin/ with zig cc. ZIG names the compiler
# (default: zig on PATH). Each build is static; CI asserts that.
set -eu
cd "$(dirname "$0")"
ZIG=${ZIG:-zig}
mkdir -p bin

COMMON="-Os -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall"

# The linker script packs everything into one PT_LOAD; the section header
# table is then stripped, since a static executable does not need it.
for t in x86_64:amd64 aarch64:arm64; do
	$ZIG cc -target "${t%%:*}-linux-musl" $COMMON -nostdlib -ffreestanding -static -fno-pic -fno-pie \
		-Wl,--gc-sections -Wl,--build-id=none -Wl,-z,norelro -Wl,-T,linux/apeld.ld -s \
		-o "bin/apeld-linux-${t##*:}" linux/apeld.c
	${STRIP:-llvm-strip} --strip-sections "bin/apeld-linux-${t##*:}"
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

# zig compiles the object; lld-link links it against an import library made
# from windows/kernel32.def. zig's own link always adds a PDB debug record,
# which is not reproducible. /Brepro derives the timestamp from the content.
$ZIG cc -target x86_64-windows-gnu $COMMON -c -o bin/apeld-windows.obj windows/apeld.c
${DLLTOOL:-llvm-dlltool} -m i386:x86-64 -d windows/kernel32.def -l bin/kernel32.lib
${LLDLINK:-lld-link} /entry:start /subsystem:console /nodefaultlib /Brepro /opt:ref \
	/merge:.rdata=.text /out:bin/apeld-windows-amd64.exe bin/apeld-windows.obj bin/kernel32.lib
rm -f bin/apeld-windows.obj bin/kernel32.lib

ls -l bin
