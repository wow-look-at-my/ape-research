#include "rs.h"
struct mh64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { unsigned int cmd, cmdsize; };
struct seg64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct dylibc { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct ldc { unsigned int cmd, cmdsize, dataoff, datasize; };
#define LC_ID_DYLIB 0xd
static int mm(unsigned long a){ if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
static int has(const char*s,const char*t){ for(;*s;s++){const char*a=s,*b=t; while(*a&&*b&&*a==*b){a++;b++;} if(!*b)return 1;} return 0; }
int main(int argc,char**argv,char**envp){
  for(unsigned long a=0x180000000UL; a<0x220000000UL; a+=0x4000){
    if(!mm(a)) continue;
    if(*(volatile unsigned int*)a!=0xfeedfacf) continue;
    struct mh64*h=(struct mh64*)a; if(h->ncmds==0||h->ncmds>8192) continue;
    unsigned long p=a+32, tv=0; const char*id="";
    for(unsigned int i=0;i<h->ncmds;i++){ struct lc*l=(struct lc*)p;
      if(l->cmd==0x19){ struct seg64*s=(struct seg64*)p; if(s->segname[0]=='_'&&s->segname[4]=='X'&&s->segname[5]=='T') tv=s->vmaddr; }
      if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p; id=(const char*)((unsigned long)d+d->nameoff); }
      if(l->cmdsize<8) break; p+=l->cmdsize;
    }
    if(has(id,"kernel")||has(id,"pthread")||has(id,"libSystem.B")||has(id,"dyld")||has(id,"libdispatch")){
      o_puth(a); o_lbl(" textvm="); o_puth(tv); o_lbl(" id="); o_puts(id); o_lbl("\n");
    }
  }
  return 0;
}
