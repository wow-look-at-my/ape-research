#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
// Classify each libSystem function: is it a thin syscall wrapper (svc within
// first ~8 insns), a mach-trap wrapper, or real C code?
static const char* names[]={
 "getpid","mmap","munmap","mprotect","mach_vm_region","fork","openat","close","write","read",
 "sigaction","pselect","pipe","getentropy","raise","getrlimit","setrlimit",
 "sem_open","sem_unlink","sem_close","sem_post","sem_wait","sem_trywait",
 "pthread_self","pthread_create","pthread_kill","pthread_sigmask","pthread_setname_np",
 "pthread_exit","pthread_join","pthread_yield_np","pthread_attr_init","pthread_attr_destroy",
 "pthread_attr_setstacksize","pthread_attr_setguardsize","pthread_cpu_number_np",
 "sys_icache_invalidate","pthread_jit_write_protect_np","pthread_jit_write_protect_supported_np",
 "sysctl","sysctlbyname","sysctlnametomib","clock_gettime","nanosleep","sigaltstack",
 "dlsym","dlopen","dlclose","dlerror",
 "dispatch_semaphore_create","dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime",
 "issetugid","thread_selfid","pthread_mutex_lock","pthread_cond_wait","malloc",0};
int main(void){
  printf("%-42s %-12s %s\n","symbol","kind","first insns");
  for(int i=0;names[i];i++){
    void*p=dlsym(RTLD_DEFAULT,names[i]);
    if(!p){ printf("%-42s %-12s\n",names[i],"ABSENT"); continue; }
    uint32_t*w=(uint32_t*)p;
    int svc=-1, movn=-1, movz16=-1;
    for(int k=0;k<12;k++){
      if(w[k]==0xd4001001 && svc<0) svc=k;
      if((w[k]&0xff800000)==0x92800000 && ((w[k]>>5)&0x1f)==16 && movn<0) movn=k;
      if((w[k]&0xff800000)==0xd2800000 && ((w[k]>>5)&0x1f)==16 && movz16<0) movz16=k;
    }
    const char*kind="real-C";
    if(movn==0||movn==1) kind="mach-svc";
    else if(svc>=0 && svc<=6) kind="direct-svc";
    else if(svc>6) kind="svc-later";
    printf("%-42s %-12s",names[i],kind);
    printf(" %08x %08x %08x %08x",w[0],w[1],w[2],w[3]);
    if(svc>=0) printf("  [svc@%d]",svc);
    if(movn>=0) printf("  [movn-x16@%d]",movn);
    printf("\n");
  }
  return 0;
}
