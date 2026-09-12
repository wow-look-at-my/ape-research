#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/wait.h>
struct mh64 { uint32_t magic,cputype,cpusubtype,filetype,ncmds,sizeofcmds,flags,reserved; };
struct lc { uint32_t cmd,cmdsize; };
struct sc { uint32_t cmd,cmdsize,symoff,nsyms,stroff,strsize; };
static int rdfork(uint64_t a,void*out,uint64_t n){ pid_t p=fork(); if(p==0){ memcpy(out,(void*)a,n); _exit(0);} int st=0;waitpid(p,&st,0); return WIFEXITED(st)?0:-1; }
int main(void){
  // Test: LC_SYMTAB for libsystem_kernel, hdr=0x188cdd000
  uint64_t hdr=0x188cdd000ULL;
  struct mh64*h=(struct mh64*)hdr;
  printf("ncmds=%u sizeofcmds=%u\n",h->ncmds,h->sizeofcmds);
  uint64_t p=hdr+32; uint32_t symoff=0,nsyms=0,stroff=0,strsize=0;
  for(uint32_t i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
    if(l->cmd==0x2){ struct sc*s=(struct sc*)p; symoff=s->symoff;nsyms=s->nsyms;stroff=s->stroff;strsize=s->strsize; }
    p+=l->cmdsize;
  }
  printf("kernel symoff=%#x nsyms=%u stroff=%#x strsize=%u\n",symoff,nsyms,stroff,strsize);
  uint64_t syms=hdr+symoff, strs=hdr+stroff;
  printf("syms=%#llx strs=%#llx\n",(unsigned long long)syms,(unsigned long long)strs);
  for(uint32_t i=0;i<nsyms && i<12;i++){
    struct { uint32_t n_strx; uint8_t n_type,n_sect; uint16_t n_desc; uint64_t n_value; } e;
    if(rdfork(syms+i*16,&e,16)){ printf("  [%u] FAULT\n",i); break; }
    char nm[256]; 
    int ok=0;
    if(!rdfork(strs+e.n_strx,nm,200)){ nm[200]=0; ok=1; }
    printf("  [%u] strx=%u type=%#x val=%#llx name=%s\n",i,e.n_strx,e.n_type,(unsigned long long)e.n_value,ok?nm:"?");
  }
  return 0;
}
