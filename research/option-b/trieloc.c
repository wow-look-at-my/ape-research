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
struct ldc { uint32_t cmd,cmdsize,dataoff,datasize; };
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
// reliable mapped read via fork
static int rdfork(uint64_t a,uint8_t*out,uint64_t n){
  pid_t p=fork(); if(p==0){ memcpy(out,(void*)a,n); _exit(0);} int st=0; waitpid(p,&st,0); return WIFEXITED(st)?0:-1;
}
int main(void){
  uint64_t lh=0x18890f000ULL; // libdyld header
  struct mh64*h=(struct mh64*)lh;
  uint32_t symoff=0,nsyms=0,stroff=0,strsize=0,dataoff=0;
  for(uint32_t i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)(lh+32); 
    // walk properly
  }
  uint64_t p=lh+32;
  for(uint32_t i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
    if(l->cmd==0x2){ struct sc*s=(struct sc*)p; symoff=s->symoff;nsyms=s->nsyms;stroff=s->stroff;strsize=s->strsize; }
    if(l->cmd==0x80000033||l->cmd==0x80000022){ struct ldc*d=(struct ldc*)p; dataoff=d->dataoff; }
    p+=l->cmdsize;
  }
  printf("libdyld: symoff=%#x nsyms=%u stroff=%#x strsize=%u dataoff=%#x\n",symoff,nsyms,stroff,strsize,dataoff);
  uint64_t slide=0x8830000ULL;
  uint64_t cands[]={ lh+dataoff, lh+dataoff-slide, 0x180000000ULL+dataoff+slide, 0x1ff05c000ULL+dataoff-0x4000, 0x1ff05c000ULL+slide+dataoff-0x4000, 0x188830000ULL+dataoff, 0};
  const char*nm[]={"hdr+off","hdr+off-slide","vm0+off+slide","ldvm+off-ldfo","ldvm+slide+off-ldfo","cachebase+off"};
  for(int i=0;cands[i];i++){
    uint8_t b[16]; int r=rdfork(cands[i],b,16);
    printf("  %-20s %#llx read=%s bytes=",nm[i],(unsigned long long)cands[i],r?"FAULT":"ok");
    if(!r) for(int k=0;k<16;k++) printf("%02x ",b[k]);
    printf("\n"); fflush(stdout);
  }
  // symtab candidate
  printf("--- symtab test ---\n");
  uint64_t scands[]={lh+symoff, 0x180000000ULL+symoff+slide, 0x188830000ULL+symoff, 0};
  const char*snm[]={"hdr+symoff","vm0+symoff+slide","cachebase+symoff"};
  for(int i=0;scands[i];i++){
    uint8_t b[32]; int r=rdfork(scands[i],b,32);
    printf("  %-20s %#llx read=%s\n",snm[i],(unsigned long long)scands[i],r?"FAULT":"ok");
    if(!r){ for(int k=0;k<nsyms&&k<4;k++){ uint32_t n_strx=*(uint32_t*)(b+k*16); if(n_strx<strsize) printf("    [%d] strx=%u\n",k,n_strx); } }
    fflush(stdout);
  }
  return 0;
}
