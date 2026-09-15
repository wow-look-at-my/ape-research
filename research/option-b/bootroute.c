#include "rs.h"
// Zero-import bootstrap: find libSystem's Mach-O header by scanning the
// loaded address space outward from our own image, then walk its
// LC_SYMTAB and the shared cache's LC_DYLD_INFO_ONLY/export trie.
static int is_macho(unsigned long p, unsigned long *size_out, unsigned long *slide_out){
  if(p & 7) return 0;
  unsigned long m=*(volatile unsigned long*)p;
  unsigned int magic=(unsigned int)m;
  if(magic!=0xfeedfacf && magic!=0xcffaedfe && magic!=0xfeedface) return 0;
  unsigned int cputype=(unsigned int)(m>>32);
  if((cputype & 0xffffff)!=12) return 0; // CPU_TYPE_ARM
  unsigned int ncmds=(unsigned int)(*(volatile unsigned int*)(p+16));
  if(ncmds==0 || ncmds>4096) return 0;
  // walk load commands to get __TEXT size and a sanity check
  unsigned long off=p+32, end=p+32+ (unsigned long)ncmds*4096;
  unsigned long textsize=0; int ok=0;
  for(unsigned int i=0;i<ncmds;i++){
    unsigned int cmd=*(volatile unsigned int*)off;
    unsigned int cmdsz=*(volatile unsigned int*)(off+4);
    if(cmdsz<8 || cmdsz>0x10000) return 0;
    if(cmd==0x19 /*LC_SEGMENT_64*/){
      const char *seg=(const char*)(off+8);
      if(seg[0]=='_'&&seg[1]=='_'&&seg[2]=='T'&&seg[3]=='E'&&seg[4]=='X'&&seg[5]=='T'){
        textsize=*(volatile unsigned long*)(off+8+48); ok=1;
      }
    }
    off+=cmdsz;
    if(off>end) break;
  }
  if(!ok) return 0;
  if(size_out)*size_out=textsize;
  if(slide_out)*slide_out=0;
  return 1;
}
int main(int argc,char**argv,char**envp){
  o_lbl("scanning for Mach-O headers near the stack/heap...\n");
  // Scan a window of the shared-cache/loaded-image region. On arm64 macOS
  // the cache is mapped around 0x180000000+. Probe every 4K page for a
  // mach_header, tolerating unmapped pages.
  unsigned long hits=0, libsystem=0;
  for(unsigned long a=0x180000000UL; a<0x1a0000000UL; a+=0x4000){
    unsigned long sz=0;
    if(is_macho(a,&sz,0)){ hits++; 
      if(hits<=8){ o_lbl("  macho @ "); o_puth(a); o_lbl(" textsz="); o_puth(sz); o_lbl("\n"); }
    }
  }
  o_lbl("hits="); o_putn(hits); o_lbl("\n");
  (void)libsystem;
  return 0;
}
