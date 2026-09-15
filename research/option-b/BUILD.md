# Build commands

All builds use `/usr/bin/clang` and `/usr/bin/ld`. SDK:

```
SDK=$(xcrun --show-sdk-path)
```

## Common flags

```
CFLAGS="-arch arm64 -mmacosx-version-min=11.0 -O2 -ffreestanding -fno-stack-protector"
LDFLAGS="-arch arm64 -weak-lSystem -syslibroot $SDK -platform_version macos 11.0 11.0"
```

`-ffreestanding -fno-stack-protector` keeps the compiler from emitting calls to `memcpy`/`__stack_chk_fail`. Any accidental libc call shows up immediately as an undefined symbol in `nm -u`.


```
clang -c $CFLAGS -o X.o X.c
clang -c -arch arm64 -mmacosx-version-min=11.0 -o rs_syscall.o rs_syscall.s
ld $LDFLAGS -e _main -o X X.o rs_syscall.o
nm -u X          # must print nothing
```

Note: if a `main` is not the entry you want, `-e _start` plus an assembly stub (see `appleentry.s` / `entrycap.s` in the transcript) is how the LC_MAIN x3 "apple vector" was captured.

## The decisive resolver

```
clang -c $CFLAGS -o zfinal.o zfinal.c
clang -c -arch arm64 -mmacosx-version-min=11.0 -o rs_syscall.o rs_syscall.s
ld $LDFLAGS -e _main -o zfinal zfinal.o rs_syscall.o
nm -u zfinal     # empty
./zfinal
```

Slow (naive) variants: `tbase`, `zres2`, `zres3`. The tuned one is `zfinal` (prefix pruning + cache image table).

## Drop-in dlsym

```
clang -c $CFLAGS -o mydlsym.o mydlsym.c
ld $LDFLAGS -e _main -o mydlsym mydlsym.o rs_syscall.o
./mydlsym
```


```
clang -arch arm64 -mmacosx-version-min=11.0 -O1 -o verify verify.c
./verify > dsym.txt
```

## Weak/strong link matrix

Stub dylibs (so a name can be present at link time but absent at run time):

```
clang -dynamiclib -arch arm64 -mmacosx-version-min=11.0 \
  -install_name /usr/lib/libMacos.B.dylib -o stubs2/libMacos.dylib stubdylib.c
clang -dynamiclib -arch arm64 -mmacosx-version-min=11.0 \
  -install_name /usr/lib/libNope.1.dylib -o stubs2/libNope.dylib stubdylib.c
```

Cases (all with `-Lstubs2 -syslibroot $SDK -platform_version macos 11.0 11.0`):

```
A: -weak-lMacos                 (single weak, absent)      -> SIGABRT
C: -lMacos                      (single strong, absent)     -> SIGABRT
D: -weak-lMacos -weak-lNope     (two weak, both absent)     -> SIGABRT
E: -weak-lMacos -lSystem        (weak absent + strong real) -> runs
```

Rename a dylib install-name in place (keep the byte length identical, then re-sign — editing bytes invalidates the signature and the kernel SIGKILLs it):

```
codesign -f -s - <file>
```

## The hedge

```
clang -dynamiclib -arch arm64 -mmacosx-version-min=11.0 \
  -install_name @loader_path/libhedge.dylib -o hedge/libhedge.dylib hedge/libhedge.c
ld -arch arm64 -weak-lSystem -weak-lhedge -Lhedge -syslibroot $SDK \
  -platform_version macos 11.0 11.0 -e _main -o hedge/app hedge/main.o
```

## Sizes and dialect

```
size -m X
otool -l X | grep -E "cmd LC_|minos|name "
for m in 10.15 11.0 12.0 13.0 14.0 15.0 26.0; do
  ld -arch arm64 -e _main -o mv_$m min.o rs_syscall.o -weak-lSystem \
     -syslibroot $SDK -platform_version macos $m $m
done
```
