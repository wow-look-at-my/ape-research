#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string.h>
extern char **_NSGetArgv(void);
int main(void){
  printf("_dyld_get_image_vmaddr_slide(0)=%p\n", (void*)_dyld_get_image_vmaddr_slide(0));
  printf("main image header=%p name=%s\n", _dyld_get_image_header(0), _dyld_get_image_name(0));
  uint32_t n=_dyld_image_count();
  for(uint32_t i=0;i<n && i<6;i++){
    printf("img[%u] hdr=%p slide=%p name=%s\n", i,
      (void*)_dyld_get_image_header(i), (void*)_dyld_get_image_vmaddr_slide(i), _dyld_get_image_name(i));
  }
  void *ls = dlopen("/usr/lib/libSystem.B.dylib", RTLD_NOLOAD);
  printf("libSystem handle(no-load)=%p\n", ls);
  void *f = dlsym(RTLD_DEFAULT, "getpid");
  printf("getpid=%p\n", f);
  // find libSystem image
  for(uint32_t i=0;i<n;i++){
    const char*nm=_dyld_get_image_name(i);
    if(strstr(nm,"libSystem")) printf("libSystem img[%u]=%p slide=%p name=%s\n",i,(void*)_dyld_get_image_header(i),(void*)_dyld_get_image_vmaddr_slide(i),nm);
  }
  return 0;
}
