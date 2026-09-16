# The precompiled loader on an Apple Silicon host.
tests:
	- desc: stock shell boot compiles ape-m1.c with cc (baseline)
	  cmd: sh out/probe.com hello
	  timeout: 120s
	  outputs:
		stdout:
			- '/probe.com" "hello"]'
			- 'goos=darwin goarch=arm64'

	- desc: loader maps the arm64 payload with args and env intact
	  cmd: bin/apeld-darwin-arm64 out/probe.com hello world
	  inputs:
		env:
			APE_PROBE_ENV: yes
	  outputs:
		stdout:
			- 'args=["out/probe.com" "hello" "world"]'
			- '/out/probe.com" exeErr=<nil>'
			- 'env="yes"'
			- 'goos=darwin goarch=arm64'
		stderr: []

	- desc: exit status passes through
	  cmd: bin/apeld-darwin-arm64 out/probe.com fail
	  exit: 3

	- desc: a missing program fails loudly
	  cmd: bin/apeld-darwin-arm64 /nonexistent.com
	  exit: 127
	  outputs:
		stderr:
			- 'apeld: cannot open program'

	- desc: a file with no arm64 payload is refused
	  cmd: bin/apeld-darwin-arm64 bin/apeld-darwin-arm64
	  exit: 127
	  outputs:
		stderr:
			- 'no arm64 payload'

	# This host has no tmpfs, so a caller that unpacks a throwaway loader puts
	# it on a disk. -u takes it off again before the payload starts. XNU refuses
	# exec through /dev/fd, so unlinking first is not available here.
	- desc: -u removes the loader's own file, and still runs the program
	  cmd: |
		cp bin/apeld-darwin-arm64 "$TMPDIR/apeld-darwin-arm64"
		chmod 755 "$TMPDIR/apeld-darwin-arm64"
		"$TMPDIR/apeld-darwin-arm64" -u out/probe.com
		test ! -e "$TMPDIR/apeld-darwin-arm64" || { echo "the loader survived -u" >&2; exit 1; }
	  exit: 0
	  outputs:
		stdout:
			- 'goos=darwin goarch=arm64'

	# Without it the file stays. A loader somebody installed must survive every
	# run, so the removal can never be the default.
	- desc: no -u leaves the loader where it is
	  cmd: |
		cp bin/apeld-darwin-arm64 "$TMPDIR/apeld-darwin-arm64"
		chmod 755 "$TMPDIR/apeld-darwin-arm64"
		"$TMPDIR/apeld-darwin-arm64" out/probe.com
		test -e "$TMPDIR/apeld-darwin-arm64" || { echo "the loader was removed without -u" >&2; exit 1; }
	  exit: 0
	  outputs:
		stdout:
			- 'goos=darwin goarch=arm64'
