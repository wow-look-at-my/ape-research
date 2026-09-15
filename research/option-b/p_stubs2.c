#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
static void show(const char*n){
  void*p=dlsym(RTLD_DEFAULT,n);
  if(!p){ printf("%-40s NULL\n",n); return; }
  uint32_t *c=(uint32_t*)p;
  printf("%s ", n);
  for(int i=0;i<6;i++) printf("%08x ", c[i]);
  printf("\n");
}
int main(void){
  const char*names[]={
   "task_self_trap","host_self_trap","thread_self_trap","mach_reply_port",
   "mach_msg_trap","mach_msg_overwrite_trap",
   "semaphore_signal_trap","semaphore_signal_all_trap","semaphore_signal_thread_trap",
   "semaphore_wait_trap","semaphore_wait_signal_trap","semaphore_timedwait_trap","semaphore_timedwait_signal_trap",
   "swtch_pri","swtch","thread_switch","thread_depress_abort","thread_get_special_reply_port",
   "_kernelrpc_mach_vm_allocate_trap","_kernelrpc_mach_vm_deallocate_trap","_kernelrpc_mach_vm_protect_trap",
   "_kernelrpc_mach_vm_map_trap","_kernelrpc_mach_port_allocate_trap","_kernelrpc_mach_port_deallocate_trap",
   "_kernelrpc_mach_port_insert_right_trap","_kernelrpc_mach_port_mod_refs_trap",
   "_kernelrpc_mach_port_construct_trap","_kernelrpc_mach_port_destruct_trap",
   "mach_vm_region","mach_vm_region_recurse","mach_vm_allocate","mach_vm_deallocate","mach_vm_protect",
   "thread_get_special_reply_port","thread_set_special_reply_port","task_threads",
   0};
  for(int i=0;names[i];i++) show(names[i]);
  return 0;
}
