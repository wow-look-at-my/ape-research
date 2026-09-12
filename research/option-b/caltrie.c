#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <sys/syscall.h>
#include <unistd.h>
struct mh64 { uint32_t magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { uint32_t cmd, cmdsize; };
struct dylibc { uint32_t cmd, cmdsize, nameoff, timestamp, curver, compatver; };
struct ldc { uint32_t cmd, cmdsize, dataoff, datasize; };
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static uint64_t uleb(const uint8_t**pp){ uint64_t r=0; int s=0; const uint8_t*p=*pp;
  for(int i=0;i<10;i++){ uint8_t b=*p++; r|=(uint64_t)(b&0x7f)<<s; if(!(b&0x80))break; s+=7; } *pp=p; return r; }
struct tctx{uint64_t base;const char*want;int wlen;int found;uint64_t off;int steps;};
static void twalk(const uint8_t*node,char*pfx,int plen,struct tctx*c){
  if(c->found||plen>240||c->steps++>2000000) return;
  const uint8_t*p=node;
  uint64_t term=uleb(&p);
  if(term&&plen==c->wlen){
    if(!memcmp(pfx,c->want,plen)){ const uint8_t*a=p; uint64_t flags=uleb(&a);
      if(!(flags&0x08)){ c->off=uleb(&a); c->found=1; } } }
  uint64_t nc=*p++;
  for(uint64_t i=0;i<nc;i++){
    const uint8_t*s=p; while(*s)s++; uint64_t el=(uint64_t)(s-p); s++;
    uint64_t co=uleb(&s);
    if(plen+(int)el<240){ memcpy(pfx+plen,p,el);
      if(mapped(c->base+co)) twalk((const uint8_t*)(c->base+co),pfx,plen+(int)el,c); }
    p=s;
  }
}
static uint64_t lookup(uint64_t trie,const char*name){
  if(!mapped(trie)) return 0;
  struct tctx c={trie,name,(int)strlen(name),0,0,0}; char pfx[256];
  twalk((const uint8_t*)trie,pfx,0,&c);
  return c.found?trie+c.off:0;
}
int main(void){
  char cmd[512];
  // find kernel image header + dataoff via dyld
  uint32_t n=_dyld_image_count();
  const struct mach_header_64 *kh=0, *lh=0;
  uint64_t kslide=0, lslide=0; uint32_t kdo=0, ldo=0;
  for(uint32_t i=0;i<n;i++){
    const char*nm=_dyld_get_image_name(i);
    if(!nm) continue;
    if(!strstr(nm,"libsystem_kernel")) continue;
    kh=(const struct mach_header_64*)_dyld_get_image_header(i); kslide=_dyld_get_image_vmaddr_slide(i);
    const uint8_t*p=(const uint8_t*)kh+sizeof(*kh);
    for(uint32_t c=0;c<kh->ncmds;c++){ const struct lc*l=(const struct lc*)p;
      if(l->cmd==0x80000033||l->cmd==0x80000022){ const struct ldc*d=(const struct ldc*)p; kdo=d->dataoff; }
      p+=l->cmdsize; }
  }
  printf("kernel hdr=%p slide=%#llx dataoff=%#x\n",(void*)kh,(unsigned long long)kslide,kdo); fflush(stdout);
  // cache base: lowest shared cache mapping. Find via the image: hdr - (vmaddr - cache_vm0)?
  // Instead brute: try formulas.
  uint64_t cands[6];
  cands[0]=(uint64_t)kh + kdo;
  cands[1]=kslide + kdo;
  cands[2]=(uint64_t)kh - 0x1804ad000ULL + kdo;  // hdr - textvm + dataoff? (textvm unslid)
  cands[3]=(uint64_t)kh + (kdo - 0x4000);        // hdr + (dataoff - linkedit fileoff)
  cands[4]=0x0; // cache base + dataoff, cache base discovered below
  const char*nm4[6]={"hdr+off","slide+off","hdr-textvm+off","hdr+(off-ldfo)","cache+off","0x1ff05c000+slide+(off-0x4000)"};
  // discover cache base by scanning down from hdr for 'dyld_v1'
  uint64_t cachebase=0;
  cachebase=0x188830000ULL;
  (void)cachebase;
  printf("cachebase=%#llx\n",(unsigned long long)cachebase);
  cands[4]=cachebase+kdo;
  cands[5]=0x1ff05c000ULL+kslide+(kdo-0x4000);
  for(int i=0;i<6;i++){
    uint64_t t=cands[i];
    uint64_t r=lookup(t,"getpid");
    printf("  %-40s trie=%#llx -> getpid=%#llx %s\n",nm4[i],(unsigned long long)t,(unsigned long long)r,
      r==0x188cde178ULL?"*** MATCH ***":"");
  }
  return 0;
}
