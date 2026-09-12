# The memfd loader on a Linux arm64 host. APELD names the loader binary.
# The working directory is mounted read-only in the sandbox, which is the
# situation the loader exists for.
tests:
	- desc: stock shell boot refuses a linux/arm64 host (baseline)
	  cmd: sh out/probe.com hello
	  exit: 1
	  outputs:
		stderr:
			- "APE: unsupported host"

	- desc: loader boots the APE from a memfd with args and env intact
	  cmd: $APELD out/probe.com hello world
	  inputs:
		env:
			APE_PROBE_ENV: yes
	  outputs:
		stdout:
			- 'args=["out/probe.com" "hello" "world"]'
			- 'exe="/memfd:probe.com" exeErr=<nil>'
			- 'env="yes"'
			- 'goos=linux goarch=arm64'
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
