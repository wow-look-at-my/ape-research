set -u
test_name() {
  local nm="$1" label="$2"
  cp wl "/tmp/apex/dt_try"
  # Write the new name into the dylinker command's name field (offset 640+12),
  # zero-filling the old string area first.
  dd if=/dev/zero of=/tmp/apex/dt_try bs=1 seek=$((640+12)) count=16 conv=notrunc 2>/dev/null
  printf '%s' "$nm" | dd of=/tmp/apex/dt_try bs=1 seek=$((640+12)) conv=notrunc 2>/dev/null
  codesign -f -s - /tmp/apex/dt_try 2>/dev/null
  out=$(/tmp/apex/dt_try 2>&1); rc=$?
  printf '%-28s rc=%-4s %s\n' "$label" "$rc" "$(echo "$out"|head -1)"
}
test_name "/usr/lib/dyld"        "exact /usr/lib/dyld"
test_name "/usr/lib/dyldX"       "trailing char changed"
test_name "/usr/lib/Dyld"        "case changed"
test_name "/usr/lib/dyld/"       "trailing slash"
test_name "./dyld"               "relative"
test_name ""                     "empty"
