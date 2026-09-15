#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <dlfcn.h>
int main(void){
  const char*syms[]={"getpid","mmap","mach_vm_region","fork","pthread_self","pthread_create","pthread_kill",
    "sysctl","sysctlbyname","getentropy","dlsym","dlopen","sys_icache_invalidate","pthread_jit_write_protect_np",
    "pthread_jit_write_protect_supported_np","sysctlnametomib","pthread_join","pthread_sigmask","pthread_setname_np",
    "pthread_exit","munmap","mprotect","openat","close","write","read","sigaction","pselect","sigaltstack",
    "getrlimit","setrlimit","sem_open","sem_unlink","sem_close","sem_post","sem_wait","sem_trywait","raise",
    "pthread_attr_init","pthread_attr_destroy","pthread_attr_setstacksize","pthread_attr_setguardsize",
    "clock_gettime","nanosleep","pipe","pthread_cpu_number_np","dlclose","dlerror","pthread_yield_np",
    "dispatch_semaphore_create","dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime",
    "pthread_stack_min","mmap",0};
  for(int i=0;syms[i];i++){
    void*p=dlsym(RTLD_DEFAULT,syms[i]);
    printf("%-42s %p\n",syms[i],p);
  }
  return 0;
}
