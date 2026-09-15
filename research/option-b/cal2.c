#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string.h>
struct seg64 { uint32_t cmd, cmdsize; char segname[16]; uint64_t vmaddr, vmsize, fileoff, filesize; int32_t maxprot, initprot; uint32_t nsects, flags; };
int main(void){
  void *f = dlsym(RTLD_DEFAULT, "getpid");
  void *p_ps = dlsym(RTLD_DEFAULT, "pthread_self");
  void *p_sysctl = dlsym(RTLD_DEFAULT, "sysctl");
  void *p_mmap = dlsym(RTLD_DEFAULT, "mmap");
  printf("getpid=%p pthread_self=%p sysctl=%p mmap=%p\n", f,p_ps,p_sysctl,p_mmap);
  uint32_t n=_dyld_image_count();
  for(uint32_t i=0;i<n;i++){
    const char*nm=_dyld_get_image_name(i);
    if(!nm) continue;
    if(!strstr(nm,"libsystem_kernel") && !strstr(nm,"libsystem_pthread") && !strstr(nm,"libsystem_c.dylib") && !strstr(nm,"libdyld")) continue;
    const struct mach_header_64*h=(const struct mach_header_64*)_dyld_get_image_header(i);
    int64_t slide=_dyld_get_image_vmaddr_slide(i);
    const uint8_t*p=(const uint8_t*)h+sizeof(*h);
    printf("%s hdr=%p slide=%#llx\n", nm, h, (unsigned long long)slide);
    for(uint32_t c=0;c<h->ncmds;c++){
      const struct load_command*lc=(const struct load_command*)p;
      if(lc->cmd==LC_SEGMENT_64){ const struct seg64*s=(const struct seg64*)p;
        printf("   seg %.16s vm=%#llx fileoff=%#llx vmsize=%#llx\n", s->segname,
          (unsigned long long)s->vmaddr,(unsigned long long)s->fileoff,(unsigned long long)s->vmsize); }
      if(lc->cmd==LC_DYLD_INFO_ONLY || lc->cmd==LC_DYLD_EXPORTS_TRIE){
        const struct linkedit_data_command*d=(const struct linkedit_data_command*)p;
        printf("   trie cmd=%#x dataoff=%#x datasize=%u -> hdr+off=%p\n", lc->cmd, d->dataoff, d->datasize, (void*)((uint8_t*)h+d->dataoff));
      }
      p+=lc->cmdsize;
    }
  }
  return 0;
}
