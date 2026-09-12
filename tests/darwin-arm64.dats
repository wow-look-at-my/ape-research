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
