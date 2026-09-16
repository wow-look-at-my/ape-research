# The memfd loader on a Linux amd64 host. APELD names the loader binary.
# The working directory is mounted read-only in the sandbox, which is the
# situation the loader exists for.
tests:
	- desc: stock shell boot (baseline)
	  cmd: sh out/probe.com hello
	  outputs:
		stdout:
			- '/probe.com" "hello"]'
			- 'goos=linux goarch=amd64'

	- desc: loader boots the APE from a memfd with args and env intact
	  cmd: $APELD out/probe.com hello world
	  inputs:
		env:
			APE_PROBE_ENV: yes
	  outputs:
		stdout:
			- 'args=["out/probe.com" "hello" "world"]'
			# The loader execs a memfd, so /proc/self/exe reads back its name.
			# A runtime that resolves argv[0] instead reports the APE's own
			# path. Both are this loader working, so assert what they share.
			- 'probe.com" exeErr=<nil>'
			- 'env="yes"'
			- 'goos=linux goarch=amd64'
		stderr: []

	- desc: exit status passes through
	  cmd: $APELD out/probe.com fail
	  exit: 3

	- desc: a missing program fails loudly
	  cmd: $APELD /nonexistent.com
	  exit: 127
	  outputs:
		stderr:
			- 'apeld: cannot open program'

	- desc: no arguments is a usage error
	  cmd: $APELD
	  exit: 127
	  outputs:
		stderr:
			- 'usage'

	- desc: a file with no payload for this machine is refused
	  cmd: $APELD $APELD
	  exit: 127
	  outputs:
		stderr:
			- 'no payload for this machine'

	# A caller that unpacked a throwaway copy passes -u, so an APE leaves no
	# second file behind. The copy goes before the payload starts.
	# Each case keeps its copy in a directory of its own. These tests run at the
	# same time, and one -u run deletes any path the other one shares.
	- desc: -u removes the loader's own file, and still runs the program
	  cmd: |
		mkdir -p "$TMPDIR/u"
		cp "$APELD" "$TMPDIR/u/ld"
		chmod 755 "$TMPDIR/u/ld"
		"$TMPDIR/u/ld" -u out/probe.com
		test ! -e "$TMPDIR/u/ld" || { echo "the loader survived -u" >&2; exit 1; }
	  exit: 0
	  outputs:
		stdout:
			- 'goos=linux goarch=amd64'

	# Without it the file stays. A loader somebody installed must survive every
	# run, so the removal can never be the default.
	- desc: no -u leaves the loader where it is
	  cmd: |
		mkdir -p "$TMPDIR/keep"
		cp "$APELD" "$TMPDIR/keep/ld"
		chmod 755 "$TMPDIR/keep/ld"
		"$TMPDIR/keep/ld" out/probe.com
		test -e "$TMPDIR/keep/ld" || { echo "the loader was removed without -u" >&2; exit 1; }
	  exit: 0
	  outputs:
		stdout:
			- 'goos=linux goarch=amd64'
