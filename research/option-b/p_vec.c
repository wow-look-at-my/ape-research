#include "rs.h"
#include <stddef.h>
extern unsigned long ent_x30, ent_sp;
// Mach-O parsing structs (freestanding)
struct load_command { unsigned int cmd, cmdsize; };
struct mach_header_64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct segment_command_64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct symtab_command { unsigned int cmd, cmdsize, symoff, nsyms, stroff, strsize; };
struct dysymtab_command { unsigned int cmd, cmdsize; unsigned int ilocalsym,nlocalsym,iextdefsym,nextdefsym,iundefsym,nundefsym,
  tocoff,ntoc,modtaboff,nmodtab,extrefsymoff,nextrefsyms,indirectsymoff,nindirectsyms,extreloff,nextrel,locreloff,nlocrel; };
struct linkedit_data_command { unsigned int cmd, cmdsize, dataoff, datasize; };
#define LC_SEGMENT_64 0x19
#define LC_SYMTAB 0x2
#define LC_DYSYMTAB 0xb
#define LC_DYLD_INFO_ONLY 0x80000022
#define LC_DYLD_CHAINED_FIXUPS 0x80000034
#define LC_DYLD_EXPORTS_TRIE 0x80000033

static int is_macho(unsigned long p){
  if(p&7) return 0;
  unsigned int m=*(volatile unsigned int*)p;
  if(m!=0xfeedfacf && m!=0xfeedface) return 0;
  struct mach_header_64*h=(struct mach_header_64*)p;
  return h->ncmds>0 && h->ncmds<8192;
}
static void dump_lc(unsigned long base){
  struct mach_header_64*h=(struct mach_header_64*)base;
  o_lbl("  macho @"); o_puth(base); o_lbl(" ncmds="); o_putn(h->ncmds); o_lbl(" sizeofcmds="); o_putn(h->sizeofcmds); o_lbl("\n");
  unsigned long p=base+32;
  for(unsigned int i=0;i<h->ncmds;i++){
    struct load_command*lc=(struct load_command*)p;
    o_lbl("    cmd="); o_puth(lc->cmd); o_lbl(" sz="); o_putn(lc->cmdsize);
    if(lc->cmd==LC_SEGMENT_64){ struct segment_command_64*s=(struct segment_command_64*)p;
      o_lbl(" seg="); o_write(1,s->segname,16); o_lbl(" vm="); o_puth(s->vmaddr); o_lbl(" sz="); o_puth(s->vmsize);
    }
    if(lc->cmd==LC_SYMTAB){ struct symtab_command*s=(struct symtab_command*)p;
      o_lbl(" symoff="); o_puth(s->symoff); o_lbl(" nsyms="); o_putn(s->nsyms); o_lbl(" stroff="); o_puth(s->stroff); o_lbl(" strsize="); o_putn(s->strsize);
    }
    o_lbl("\n");
    if(lc->cmdsize<8) break;
    p+=lc->cmdsize;
  }
}
int main(int argc,char**argv,char**envp){
  o_lbl("LR="); o_puth(ent_x30); o_lbl(" sp="); o_puth(ent_sp); o_lbl("\n");
  // Scan the stack for pointers whose target is a Mach-O
  unsigned long *s=(unsigned long*)ent_sp;
  for(unsigned long off=0; off<0x4000; off+=8){
    unsigned long w=s[off/8];
    if(w<0x100000000UL || w>0x8000000000UL) continue;
    if(is_macho(w)){ o_lbl("stack["); o_puth(off); o_lbl("] -> macho "); o_puth(w); o_lbl("\n"); dump_lc(w); }
  }
  // Scan image region for libSystem: walk from the LR's page downward using mincore
  o_lbl("scan down from LR page for headers:\n");
  unsigned long a = ent_x30 & ~0x3fffUL;
  for(int i=0;i<2000 && a>0x180000000UL; i++, a-=0x4000){
    char vec[4]; vec[0]=0;
    if(rs3(78,a,16384,(long)vec)!=0) break;
    if((unsigned char)vec[0]&0x80) continue;
    if(is_macho(a)){ o_lbl("  macho "); o_puth(a); dump_lc(a); 
      // only report the first few
    }
  }
  return 0;
}
