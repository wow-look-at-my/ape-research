#include "rs.h"
struct mh64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct lc { unsigned int cmd, cmdsize; };
struct seg64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct dylibc { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct ldc { unsigned int cmd, cmdsize, dataoff, datasize; };
struct dic { unsigned int cmd, cmdsize, ro, rs, bo, bs, wbo, wbs, lbo, lbs, eo, es; };
struct sc { unsigned int cmd, cmdsize, symoff, nsyms, stroff, strsize; };
#define LC_SEGMENT_64 0x19
#define LC_ID_DYLIB 0xd
#define LC_DYLD_INFO_ONLY 0x80000022
#define LC_DYLD_EXPORTS_TRIE 0x80000033
#define LC_SYMTAB 0x2
static int mm(unsigned long a){ if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
int main(int argc,char**argv,char**envp){
  // Look at the libSystem header specifically and print all commands+ids
  unsigned long a=0x198b70000UL;
  if(!mm(a)){ o_lbl("unmapped\n"); return 0; }
  struct mh64*h=(struct mh64*)a;
  o_lbl("hdr magic="); o_puth(h->magic); o_lbl(" ncmds="); o_putn(h->ncmds); o_lbl("\n");
  unsigned long p=a+32;
  for(unsigned int i=0;i<h->ncmds;i++){
    struct lc*l=(struct lc*)p;
    o_lbl("  cmd="); o_puth(l->cmd); o_lbl(" sz="); o_putn(l->cmdsize);
    if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p; o_lbl(" id="); o_puts((const char*)((unsigned long)d+d->nameoff)); }
    if(l->cmd==LC_SEGMENT_64){ struct seg64*s=(struct seg64*)p; o_lbl(" "); o_write(1,s->segname,16); o_lbl(" vm="); o_puth(s->vmaddr); o_lbl(" fo="); o_puth(s->fileoff); o_lbl(" vsz="); o_puth(s->vmsize);}
    if(l->cmd==LC_DYLD_EXPORTS_TRIE||l->cmd==LC_DYLD_INFO_ONLY){ struct ldc*d=(struct ldc*)p; o_lbl(" dataoff="); o_puth(d->dataoff); o_lbl(" datasize="); o_putn((long)d->datasize); }
    if(l->cmd==LC_SYMTAB){ struct sc*s=(struct sc*)p; o_lbl(" symoff="); o_puth(s->symoff); o_lbl(" nsyms="); o_putn((long)s->nsyms); o_lbl(" stroff="); o_puth(s->stroff); }
    o_lbl("\n");
    if(l->cmdsize<8) break; p+=l->cmdsize;
  }
  // Dump first 64 bytes of the presumed trie at tv+off
  { unsigned long tv=0x190340000UL; unsigned long off=0x10e6488UL; unsigned long c=tv+off;
    o_lbl("trie cand "); o_puth(c); o_lbl(": ");
    if(mm(c)){ unsigned char*b=(unsigned char*)c; for(int i=0;i<48;i++){ o_puth(b[i]); o_lbl(" "); } o_lbl("\n"); }
    else o_lbl("unmapped\n");
  }
  return 0;
}
