#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
extern unsigned long ent_x30, ent_sp;
int main(int argc,char**argv,char**envp){
  setvbuf(stdout,NULL,_IONBF,0);
  printf("LR(x30)=%#lx sp=%#lx\n", ent_x30, ent_sp);
  uint32_t n=_dyld_image_count();
  typedef struct { unsigned long lo, hi; const char*name; } Img;
  Img imgs[64]; uint32_t m=0;
  for(uint32_t i=0;i<n && m<64;i++){
    const struct mach_header_64*h=(const struct mach_header_64*)_dyld_get_image_header(i);
    if(!h) continue;
    unsigned long lo=~0UL,hi=0;
    const uint8_t*p=(const uint8_t*)h+sizeof(*h);
    for(uint32_t c=0;c<h->ncmds;c++){
      const struct load_command*lc=(const struct load_command*)p;
      if(lc->cmd==LC_SEGMENT_64){ const struct segment_command_64*s=(const struct segment_command_64*)p;
        if(s->vmsize){ unsigned long l=s->vmaddr, u=s->vmaddr+s->vmsize; if(l<lo)lo=l; if(u>hi)hi=u; } }
      p+=lc->cmdsize;
    }
    imgs[m].lo=lo; imgs[m].hi=hi; imgs[m].name=_dyld_get_image_name(i); m++;
  }
  unsigned long *s=(unsigned long*)ent_sp;
  for(unsigned long off=0; off<0x1000; off+=8){
    unsigned long w=s[off/8];
    if(w>=0x100000000UL && w<0x300000000UL){
      const char*who="?";
      for(uint32_t i=0;i<m;i++) if(w>=imgs[i].lo && w<imgs[i].hi){ who=imgs[i].name; break; }
      if(who[0]!='?') printf("  sp+%#lx = %#lx  %s\n", off, w, who);
    }
  }
  printf("--- mincore probe test ---\n");
  unsigned long probes[]={0x180000000UL,0x188830000UL,0x188944000UL,0x198b70000UL,0x100000000UL,0xdead0000UL,0};
  char vec[8];
  for(int i=0;probes[i];i++){
    errno=0; int r=mincore((void*)probes[i], 16384, vec);
    printf("  mincore(%#lx)=%d errno=%d vec0=%u\n", probes[i], r, errno, vec[0]);
  }
  printf("--- mach_vm_region trap candidates: test via direct call ---\n");
  return 0;
}
