set -u
test_path() {
  local nm="$1" label="$2"
  cp wl /tmp/apex/dt2
  dd if=/dev/zero of=/tmp/apex/dt2 bs=1 seek=$((640+12)) count=24 conv=notrunc 2>/dev/null
  printf '%s' "$nm" | dd of=/tmp/apex/dt2 bs=1 seek=$((640+12)) conv=notrunc 2>/dev/null
  codesign -f -s - /tmp/apex/dt2 2>/dev/null
  out=$(/tmp/apex/dt2 2>&1); rc=$?
  printf '%-34s rc=%-4s %s\n' "$label" "$rc" "$(echo "$out"|head -1)"
}
test_path "/usr/lib/dyld"           "real dyld"
test_path "/usr/lib/libSystem.B.dylib" "an existing non-dyld file"
test_path "/bin/ls"                 "another existing file"
test_path "/nonexistent/thing"      "nonexistent"
