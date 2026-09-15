#include "rs.h"
struct mach_header_64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct load_command { unsigned int cmd, cmdsize; };
struct segment_command_64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct dylib_command { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct dyld_info_command { unsigned int cmd, cmdsize, rebase_off, rebase_size, bind_off, bind_size, weak_bind_off, weak_bind_size, lazy_bind_off, lazy_bind_size, export_off, export_size; };
#define LC_SEGMENT_64 0x19
#define LC_ID_DYLIB 0xd
#define LC_LOAD_DYLIB 0xc
#define LC_DYLD_INFO_ONLY 0x80000022
#define LC_DYLD_EXPORTS_TRIE 0x80000033
static int is_macho(unsigned long p){
  if(p&7) return 0;
  if(*(volatile unsigned int*)p!=0xfeedfacf) return 0;
  struct mach_header_64*h=(struct mach_header_64*)p;
  return h->ncmds>0 && h->ncmds<8192 && h->sizeofcmds>0 && h->sizeofcmds<0x20000;
}
static const char* imgname(unsigned long base, unsigned long *slide, unsigned long *exp_off, unsigned long *exp_sz, int*has_exp, int*has_trie){
  struct mach_header_64*h=(struct mach_header_64*)base;
  unsigned long p=base+32; const char*id="";
  *slide=0; *has_exp=0; *has_trie=0;
  for(unsigned int i=0;i<h->ncmds;i++){
    struct load_command*lc=(struct load_command*)p;
    if(lc->cmd==LC_SEGMENT_64){ struct segment_command_64*s=(struct segment_command_64*)p;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='T'&&s->segname[3]=='E'&&s->segname[6]=='_')
        if(s->vmaddr) *slide = base - (s->vmaddr - 0); // rough; fixed below
    }
    if(lc->cmd==LC_ID_DYLIB){ struct dylib_command*d=(struct dylib_command*)p; id=(const char*)((unsigned long)d+d->nameoff); }
    if(lc->cmd==LC_DYLD_INFO_ONLY){ struct dyld_info_command*d=(struct dyld_info_command*)p; *exp_off=d->export_off; *exp_sz=d->export_size; *has_exp=1; }
    if(lc->cmd==LC_DYLD_EXPORTS_TRIE){ struct dyld_info_command*d=(struct dyld_info_command*)p; *exp_off=d->export_off; *exp_sz=d->export_size; *has_trie=1; }
    if(lc->cmdsize<8) break;
    p+=lc->cmdsize;
  }
  return id;
}
// proper slide: find __TEXT vmaddr
static unsigned long text_vmaddr(unsigned long base){
  struct mach_header_64*h=(struct mach_header_64*)base;
  unsigned long p=base+32;
  for(unsigned int i=0;i<h->ncmds;i++){
    struct load_command*lc=(struct load_command*)p;
    if(lc->cmd==LC_SEGMENT_64){ struct segment_command_64*s=(struct segment_command_64*)p;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='T'&&s->segname[3]=='E'&&s->segname[4]=='X'&&s->segname[5]=='T')
        return s->vmaddr;
    }
    if(lc->cmdsize<8) break;
    p+=lc->cmdsize;
  }
  return 0;
}
static long getpid_cached=0;
__attribute__((noinline)) static long gp(void){ return rs0(20); }
int main(int argc,char**argv,char**envp){
  o_lbl("scanning shared cache for Mach-O headers (mincore-probed)\n");
  int found=0;
  for(unsigned long a=0x180000000UL; a<0x200000000UL && found<4000; a+=0x4000){
    char vec[2]; vec[0]=0;
    if(rs3(78,a,16384,(long)vec)!=0) continue;
    if(((unsigned char)vec[0])&0x80) continue;
    if(!is_macho(a)) continue;
    unsigned long sl,eo,es; int he,ht;
    const char*id=imgname(a,&sl,&eo,&es,&he,&ht);
    unsigned long tv=text_vmaddr(a);
    unsigned long slide=a-tv;
    found++;
    // print only interesting ones (libSystem / c / pthread / dyld) and first few
    int interesting = 0;
    for(const char*q=id;*q;q++){ if(q[0]=='l'&&q[1]=='i'&&q[2]=='b'&&(q[3]=='S'||q[3]=='s'||q[3]=='d')) {interesting=1;break;} }
    if(interesting || found<=5){
      o_lbl("found "); o_puth(a); o_lbl(" slide="); o_puth(slide); o_lbl(" id="); o_puts(id);
      o_lbl(" dyldinfo="); o_putn(he); o_lbl(" trie="); o_putn(ht);
      o_lbl(" exp_off="); o_puth(eo); o_lbl(" exp_sz="); o_puth(es); o_lbl("\n");
    }
  }
  o_lbl("total macho found="); o_putn(found); o_lbl("\n");
  return 0;
}
