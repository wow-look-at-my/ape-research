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
static int ismo(unsigned long p){ if(!mm(p))return 0; if(*(volatile unsigned int*)p!=0xfeedfacf) return 0; struct mh64*h=(struct mh64*)p; return h->ncmds>0&&h->ncmds<8192&&h->sizeofcmds>0&&h->sizeofcmds<0x20000; }
static int streq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
int main(int argc,char**argv,char**envp){
  for(unsigned long a=0x180000000UL; a<0x200000000UL; a+=0x4000){
    if(!ismo(a)) continue;
    struct mh64*h=(struct mh64*)a; unsigned long p=a+32; unsigned long tv=0, ld_vm=0, ld_fo=0;
    unsigned long trie_off=0, trie_sz=0, symoff=0, nsyms=0, stroff=0; const char*id="";
    for(unsigned int i=0;i<h->ncmds;i++){
      struct lc*l=(struct lc*)p;
      if(l->cmd==LC_SEGMENT_64){ struct seg64*s=(struct seg64*)p;
        if(s->segname[0]=='_'&&s->segname[4]=='X'&&s->segname[5]=='T') tv=s->vmaddr;
        if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='L'){ ld_vm=s->vmaddr; ld_fo=s->fileoff; } }
      if(l->cmd==LC_ID_DYLIB){ struct dylibc*d=(struct dylibc*)p; id=(const char*)((unsigned long)d+d->nameoff); }
      if(l->cmd==LC_DYLD_EXPORTS_TRIE){ struct ldc*d=(struct ldc*)p; trie_off=d->dataoff; trie_sz=d->datasize; }
      if(l->cmd==LC_DYLD_INFO_ONLY){ struct dic*d=(struct dic*)p; if(!trie_off){trie_off=d->eo;trie_sz=d->es;} }
      if(l->cmd==LC_SYMTAB){ struct sc*s=(struct sc*)p; symoff=s->symoff; nsyms=s->nsyms; stroff=s->stroff; }
      if(l->cmdsize<8) break; p+=l->cmdsize;
    }
    if(!streq(id,"/usr/lib/libSystem.B.dylib")) continue;
    o_lbl("libSystem found @"); o_puth(a); o_lbl(" __TEXT.vmaddr="); o_puth(tv); o_lbl(" slide="); o_puth(a-tv); o_lbl("\n");
    o_lbl("  __LINKEDIT vm="); o_puth(ld_vm); o_lbl(" fileoff="); o_puth(ld_fo); o_lbl("\n");
    o_lbl("  trie_off="); o_puth(trie_off); o_lbl(" sz="); o_putn((long)trie_sz); o_lbl("\n");
    o_lbl("  symoff="); o_puth(symoff); o_lbl(" nsyms="); o_putn((long)nsyms); o_lbl(" stroff="); o_puth(stroff); o_lbl("\n");
    // candidate trie addresses
    unsigned long cands[4]={a+trie_off, tv+trie_off, ld_vm + (trie_off - ld_fo), 0x180000000UL+trie_off};
    const char*names[4]={"base+off","vmaddr+off","ldvm+(off-ldfo)","dsc+off"};
    for(int i=0;i<4;i++){
      unsigned long c=cands[i];
      if(!mm(c)){ o_lbl("  cand "); o_puts(names[i]); o_lbl(" "); o_puth(c); o_lbl(" UNMAPPED\n"); continue; }
      unsigned char*b=(unsigned char*)c;
      // root node: terminalSize uleb then childCount byte
      unsigned long t=0; { unsigned char*q=b; int sh=0; for(int k=0;k<5;k++){ unsigned char x=*q++; t|=(unsigned long)(x&0x7f)<<sh; if(!(x&0x80))break; sh+=7; } }
      o_lbl("  cand "); o_puts(names[i]); o_lbl(" "); o_puth(c); o_lbl(" rootTerminal="); o_putn((long)t);
      o_lbl(" bytes="); o_puth(b[0]); o_lbl(" "); o_puth(b[1]); o_lbl(" "); o_puth(b[2]); o_lbl(" "); o_puth(b[3]); o_lbl("\n");
    }
    return 0;
  }
  o_lbl("libSystem not found\n");
  return 0;
}
