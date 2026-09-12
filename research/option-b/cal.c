#include "rs.h"
struct mh64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { unsigned int cmd, cmdsize; };
struct seg64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct dylibc { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct ldc { unsigned int cmd, cmdsize, dataoff, datasize; };
#define LC_SEGMENT_64 0x19
#define LC_ID_DYLIB 0xd
#define LC_DYLD_EXPORTS_TRIE 0x80000033
static int mm(unsigned long a){ if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
static int seq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
int main(int argc,char**argv,char**envp){
  // find libsystem_kernel and libsystem_pthread headers
  const char*want1="/usr/lib/system/libsystem_kernel.dylib";
  const char*want2="/usr/lib/system/libsystem_pthread.dylib";
  for(unsigned long a=0x180000000UL; a<0x1a0000000UL; a+=0x4000){
    if(!mm(a)) continue;
    if(*(volatile unsigned int*)a!=0xfeedfacf) continue;
    struct mh64*h=(struct mh64*)a; if(h->ncmds==0||h->ncmds>8192) continue;
    unsigned long p=a+32, tv=0, trie_off=0; const char*id="";
    for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
      if(l->cmd==LC_SEGMENT_64){ struct seg64*s=(struct seg64*)p; if(s->segname[0]=='_'&&s->segname[4]=='X'&&s->segname[5]=='T') tv=s->vmaddr; }
      if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p; id=(const char*)((unsigned long)d+d->nameoff); }
      if(l->cmd==LC_DYLD_EXPORTS_TRIE){ struct ldc*d=(struct ldc*)p; trie_off=d->dataoff; }
      if(l->cmdsize<8) break; p+=l->cmdsize;
    }
    if(seq(id,want1)||seq(id,want2)){
      o_lbl("hdr="); o_puth(a); o_lbl(" textvm="); o_puth(tv); o_lbl(" trie_off="); o_puth(trie_off); o_lbl(" id="); o_puts(id); o_lbl("\n");
    }
  }
  return 0;
}
