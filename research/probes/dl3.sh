set -u
test_path() {
  local nm="$1" label="$2"
  cp /tmp/apex/wl /tmp/apex/dt3
  dd if=/dev/zero of=/tmp/apex/dt3 bs=1 seek=$((640+12)) count=32 conv=notrunc 2>/dev/null
  printf '%s' "$nm" | dd of=/tmp/apex/dt3 bs=1 seek=$((640+12)) conv=notrunc 2>/dev/null
  codesign -f -s - /tmp/apex/dt3 2>/dev/null
  out=$(/tmp/apex/dt3 2>&1); rc=$?
  printf '%-32s rc=%-4s %s\n' "$label" "$rc" "$(echo "$out"|head -1)"
}
test_path "/tmp/apex/dyld_copy"  "a copy of dyld elsewhere"
test_path "/usr/lib/dyld"        "the canonical path"
