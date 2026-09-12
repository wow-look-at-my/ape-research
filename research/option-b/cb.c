#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <dlfcn.h>
int main(void){
  // find cache base by scanning down from 0x190000000
  uint64_t cb=0;
  for(uint64_t a=0x190000000ULL;a>0x180000000ULL;a-=0x4000){
    char v[2]; v[0]=0;
    if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) continue;
    if(((unsigned char)v[0]&0x80)) continue;
    if(*(volatile uint32_t*)a==0x646c7964){ cb=a; break; }
  }
  printf("cachebase=%#llx getpid=%p libsystem_hdr=%p\n",(unsigned long long)cb,dlsym(RTLD_DEFAULT,"getpid"),(void*)dlopen("/usr/lib/libSystem.B.dylib",RTLD_NOLOAD));
  return 0;
}
