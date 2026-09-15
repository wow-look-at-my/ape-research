#!/bin/bash
# Extract the arm64 slice of a Mach-O file using lipo, then dump its export trie.
set -e
f="$1"
out=/tmp/apex-a/slice.bin
lipo -thin arm64e "$f" -output "$out" 2>/dev/null || lipo -thin arm64 "$f" -output "$out" 2>/dev/null || cp "$f" "$out"
/tmp/apex-a/triedump "$out"
