#include <stdio.h>
#include <stdint.h>
struct S { int32_t magic, version; uintptr_t f[55]; };
int main(void){
  const char*n[]={"fork","pipe","clock_gettime","nanosleep","mmap","jit_write_protect_supported_np",
  "jit_write_protect_np","sys_icache_invalidate","pthread_create","pthread_exit","pthread_kill",
  "pthread_sigmask","pthread_setname_np","dispatch_semaphore_create","dispatch_semaphore_signal",
  "dispatch_semaphore_wait","dispatch_walltime","pthread_self","dispatch_release","raise","pthread_join",
  "pthread_yield_np","pthread_stack_min","sizeof_pthread_attr_t","pthread_attr_init","pthread_attr_destroy",
  "pthread_attr_setstacksize","pthread_attr_setguardsize","exit","close","munmap","openat","write","read",
  "sigaction","pselect","mprotect","sigaltstack","getentropy","sem_open","sem_unlink","sem_close","sem_post",
  "sem_wait","sem_trywait","getrlimit","setrlimit","dlopen","dlsym","dlclose","dlerror","pthread_cpu_number_np",
  "sysctl","sysctlbyname","sysctlnametomib"};
  printf("magic 0\nversion 4\n");
  for(int i=0;i<55;i++) printf("%-38s %zu\n", n[i], 8+8*(size_t)i);
  printf("sizeof 440\n");
  return 0;
}
