# The in-memory loader on an NT host. dats runs cmd through bash (Git for
# Windows on the runner), so paths use forward slashes.
tests:
	- desc: stock PE boot (baseline, no extraction at all)
	  cmd: out/probe.com hello
	  outputs:
		stdout:
			- 'probe.com" "hello"]'
			- 'goos=windows goarch=amd64'

	- desc: loader places the payload in its own process with args and env intact
	  cmd: bin/apeld-windows-amd64.exe out/probe.com hello world
	  inputs:
		env:
			APE_PROBE_ENV: yes
	  outputs:
		stdout:
			- 'args=["out/probe.com" "hello" "world"]'
			- 'env="yes"'
			- 'goos=windows goarch=amd64'

	- desc: exit status passes through
	  cmd: bin/apeld-windows-amd64.exe out/probe.com fail
	  exit: 3

	- desc: a missing program fails loudly
	  cmd: bin/apeld-windows-amd64.exe /nonexistent.com
	  exit: 127
	  outputs:
		stderr:
			- 'apeld: cannot open program'
