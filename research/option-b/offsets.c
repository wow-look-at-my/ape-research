#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
// mirror of the Go syslib struct: 2 int32 then all uintptr (8 bytes)
#define F(x) "%s %zu\n", x
int main(void){
  struct S { int32_t magic, version;
    uintptr_t fork, pipe, clock_gettime, nanosleep, mmap,
      jit_supported, jit_np, icache, pthread_create, pthread_exit, pthread_kill,
      pthread_sigmask, pthread_setname_np, dsc, dss, dsw, dwt,
      pthread_self, dispatch_release, raise, pthread_join, pthread_yield_np,
      pthread_stack_min, sizeof_pthread_attr_t, pthread_attr_init, pthread_attr_destroy,
      pthread_attr_setstacksize, pthread_attr_setguardsize,
      exit_, close_, munmap, openat, write, read, sigaction, pselect, mprotect,
      sigaltstack, getentropy, sem_open, sem_unlink, sem_close, sem_post, sem_wait,
      sem_trywait, getrlimit, setrlimit, dlopen, dlsym, dlclose, dlerror,
      pthread_cpu_number_np, sysctl, sysctlbyname, sysctlnametomib; };
  printf(F("magic"),"magic",offsetof(struct S,magic));
  printf(F("version"),"version",offsetof(struct S,version));
  printf(F("fork"),"fork",offsetof(struct S,fork));
  printf(F("pipe"),"pipe",offsetof(struct S,pipe));
  printf(F("clock_gettime"),"clock_gettime",offsetof(struct S,clock_gettime));
  printf(F("nanosleep"),"nanosleep",offsetof(struct S,nanosleep));
  printf(F("mmap"),"mmap",offsetof(struct S,mmap));
  printf(F("jit_supported"),"jit_supported",offsetof(struct S,jit_supported));
  printf(F("jit_np"),"jit_np",offsetof(struct S,jit_np));
  printf(F("icache"),"icache",offsetof(struct S,icache));
  printf(F("pthread_create"),"pthread_create",offsetof(struct S,pthread_create));
  printf(F("pthread_exit"),"pthread_exit",offsetof(struct S,pthread_exit));
  printf(F("pthread_kill"),"pthread_kill",offsetof(struct S,pthread_kill));
  printf(F("pthread_sigmask"),"pthread_sigmask",offsetof(struct S,pthread_sigmask));
  printf(F("pthread_setname_np"),"pthread_setname_np",offsetof(struct S,pthread_setname_np));
  printf(F("pthread_self"),"pthread_self",offsetof(struct S,pthread_self));
  printf(F("dispatch_release"),"dispatch_release",offsetof(struct S,dispatch_release));
  printf(F("raise"),"raise",offsetof(struct S,raise));
  printf(F("pthread_join"),"pthread_join",offsetof(struct S,pthread_join));
  printf(F("pthread_yield_np"),"pthread_yield_np",offsetof(struct S,pthread_yield_np));
  printf(F("pthread_stack_min"),"pthread_stack_min",offsetof(struct S,pthread_stack_min));
  printf(F("sizeof_pthread_attr_t"),"sizeof_pthread_attr_t",offsetof(struct S,sizeof_pthread_attr_t));
  printf(F("pthread_attr_init"),"pthread_attr_init",offsetof(struct S,pthread_attr_init));
  printf(F("pthread_attr_destroy"),"pthread_attr_destroy",offsetof(struct S,pthread_attr_destroy));
  printf(F("pthread_attr_setstacksize"),"pthread_attr_setstacksize",offsetof(struct S,pthread_attr_setstacksize));
  printf(F("pthread_attr_setguardsize"),"pthread_attr_setguardsize",offsetof(struct S,pthread_attr_setguardsize));
  printf(F("exit"),"exit",offsetof(struct S,exit_));
  printf(F("close"),"close",offsetof(struct S,close_));
  printf(F("munmap"),"munmap",offsetof(struct S,munmap));
  printf(F("openat"),"openat",offsetof(struct S,openat));
  printf(F("write"),"write",offsetof(struct S,write));
  printf(F("read"),"read",offsetof(struct S,read));
  printf(F("sigaction"),"sigaction",offsetof(struct S,sigaction));
  printf(F("pselect"),"pselect",offsetof(struct S,pselect));
  printf(F("mprotect"),"mprotect",offsetof(struct S,mprotect));
  printf(F("sigaltstack"),"sigaltstack",offsetof(struct S,sigaltstack));
  printf(F("getentropy"),"getentropy",offsetof(struct S,getentropy));
  printf(F("sem_open"),"sem_open",offsetof(struct S,sem_open));
  printf(F("sem_unlink"),"sem_unlink",offsetof(struct S,sem_unlink));
  printf(F("sem_close"),"sem_close",offsetof(struct S,sem_close));
  printf(F("sem_post"),"sem_post",offsetof(struct S,sem_post));
  printf(F("sem_wait"),"sem_wait",offsetof(struct S,sem_wait));
  printf(F("sem_trywait"),"sem_trywait",offsetof(struct S,sem_trywait));
  printf(F("getrlimit"),"getrlimit",offsetof(struct S,getrlimit));
  printf(F("setrlimit"),"setrlimit",offsetof(struct S,setrlimit));
  printf(F("dlopen"),"dlopen",offsetof(struct S,dlopen));
  printf(F("dlsym"),"dlsym",offsetof(struct S,dlsym));
  printf(F("dlclose"),"dlclose",offsetof(struct S,dlclose));
  printf(F("dlerror"),"dlerror",offsetof(struct S,dlerror));
  printf(F("pthread_cpu_number_np"),"pthread_cpu_number_np",offsetof(struct S,pthread_cpu_number_np));
  printf(F("sysctl"),"sysctl",offsetof(struct S,sysctl));
  printf(F("sysctlbyname"),"sysctlbyname",offsetof(struct S,sysctlbyname));
  printf(F("sysctlnametomib"),"sysctlnametomib",offsetof(struct S,sysctlnametomib));
  return 0;
}
