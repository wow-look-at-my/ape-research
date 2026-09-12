// probe reports what a loader handed the process: arguments, executable
// path, environment and platform. The CI jobs grep its output.
package main

import (
	"fmt"
	"os"
	"runtime"
)

func main() {
	exe, exeErr := os.Executable()
	wd, _ := os.Getwd()
	fmt.Printf("args=%q\n", os.Args)
	fmt.Printf("exe=%q exeErr=%v\n", exe, exeErr)
	fmt.Printf("cwd=%q\n", wd)
	fmt.Printf("env=%q\n", os.Getenv("APE_PROBE_ENV"))
	fmt.Printf("goos=%s goarch=%s ncpu=%d\n", runtime.GOOS, runtime.GOARCH, runtime.NumCPU())
	if len(os.Args) > 1 && os.Args[1] == "fail" {
		os.Exit(3)
	}
}
