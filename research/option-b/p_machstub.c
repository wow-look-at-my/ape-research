#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
static void show(const char*n, void*p){
  if(!p){ printf("%s = NULL\n",n); return; }
  uint32_t *c=(uint32_t*)p;
  printf("%-40s %p :", n, p);
  for(int i=0;i<5;i++) printf(" %08x", c[i]);
  // decode movz x16 imm16 (0xd2800010 base) or movz x16,#imm,lsl 16
  for(int i=0;i<5;i++){
    uint32_t w=c[i];
    if((w & 0xffe0001f)==0xd2800010){ unsigned imm=(w>>5)&0xffff; int sh=(w>>21)&3; printf("  [MOVZ x16,#%u lsl %d]", imm, sh*16);}
    if((w & 0xffe0001f)==0xf2800010){ unsigned imm=(w>>5)&0xffff; int sh=(w>>21)&3; printf("  [MOVK x16,#%u lsl %d]", imm, sh*16);}
    if(w==0xd4001001) printf("  [SVC]");
  }
  printf("\n");
}
int main(void){
  show("task_self_trap", dlsym(RTLD_DEFAULT,"task_self_trap"));
  show("mach_msg_trap", dlsym(RTLD_DEFAULT,"mach_msg_trap"));
  show("mach_msg_overwrite_trap", dlsym(RTLD_DEFAULT,"mach_msg_overwrite_trap"));
  show("swtch_pri", dlsym(RTLD_DEFAULT,"swtch_pri"));
  show("thread_switch", dlsym(RTLD_DEFAULT,"thread_switch"));
  show("semaphore_signal_trap", dlsym(RTLD_DEFAULT,"semaphore_signal_trap"));
  show("_kernelrpc_mach_vm_allocate_trap", dlsym(RTLD_DEFAULT,"_kernelrpc_mach_vm_allocate_trap"));
  show("_kernelrpc_mach_vm_protect_trap", dlsym(RTLD_DEFAULT,"_kernelrpc_mach_vm_protect_trap"));
  show("getpid", dlsym(RTLD_DEFAULT,"getpid"));
  show("fork", dlsym(RTLD_DEFAULT,"fork"));
  show("mmap", dlsym(RTLD_DEFAULT,"mmap"));
  show("pthread_self", dlsym(RTLD_DEFAULT,"pthread_self"));
  show("pthread_create", dlsym(RTLD_DEFAULT,"pthread_create"));
  show("sysctl", dlsym(RTLD_DEFAULT,"sysctl"));
  show("sysctlbyname", dlsym(RTLD_DEFAULT,"sysctlbyname"));
  show("sysctlnametomib", dlsym(RTLD_DEFAULT,"sysctlnametomib"));
  show("getentropy", dlsym(RTLD_DEFAULT,"getentropy"));
  show("dlopen", dlsym(RTLD_DEFAULT,"dlopen"));
  show("dlsym", dlsym(RTLD_DEFAULT,"dlsym"));
  show("_dyld_get_image_header", dlsym(RTLD_DEFAULT,"_dyld_get_image_header"));
  show("pthread_jit_write_protect_np", dlsym(RTLD_DEFAULT,"pthread_jit_write_protect_np"));
  show("sys_icache_invalidate", dlsym(RTLD_DEFAULT,"sys_icache_invalidate"));
  return 0;
}
