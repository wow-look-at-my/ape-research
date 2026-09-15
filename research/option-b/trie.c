#include "rs.h"
struct mach_header_64 { unsigned int magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; };
struct load_command { unsigned int cmd, cmdsize; };
struct segment_command_64 { unsigned int cmd, cmdsize; char segname[16]; unsigned long vmaddr, vmsize, fileoff, filesize; int maxprot, initprot; unsigned int nsects, flags; };
struct dylib_command { unsigned int cmd, cmdsize; unsigned int nameoff, timestamp, curver, compatver; };
struct linkedit_data_command { unsigned int cmd, cmdsize, dataoff, datasize; };
struct dyld_info_command { unsigned int cmd, cmdsize, rebase_off, rebase_size, bind_off, bind_size,
   weak_bind_off, weak_bind_size, lazy_bind_off, lazy_bind_size, export_off, export_size; };
#define LC_SEGMENT_64 0x19
#define LC_ID_DYLIB 0xd
#define LC_DYLD_INFO_ONLY 0x80000022
#define LC_DYLD_EXPORTS_TRIE 0x80000033
static int is_macho(unsigned long p){
  if(p&7) return 0; if(*(volatile unsigned int*)p!=0xfeedfacf) return 0;
  struct mach_header_64*h=(struct mach_header_64*)p;
  return h->ncmds>0 && h->ncmds<8192 && h->sizeofcmds>0 && h->sizeofcmds<0x20000;
}
static unsigned long text_vmaddr(unsigned long base){
  struct mach_header_64*h=(struct mach_header_64*)base; unsigned long p=base+32;
  for(unsigned int i=0;i<h->ncmds;i++){ struct load_command*lc=(struct load_command*)p;
    if(lc->cmd==LC_SEGMENT_64){ struct segment_command_64*s=(struct segment_command_64*)p;
      if(s->segname[0]=='_'&&s->segname[1]=='_'&&s->segname[2]=='T'&&s->segname[3]=='E'&&s->segname[4]=='X'&&s->segname[5]=='T') return s->vmaddr; }
    if(lc->cmdsize<8) break; p+=lc->cmdsize; }
  return 0;
}
static const char* get_id(unsigned long base, unsigned long *trie_off, unsigned long *trie_sz){
  struct mach_header_64*h=(struct mach_header_64*)base; unsigned long p=base+32; const char*id="";
  *trie_off=*trie_sz=0;
  for(unsigned int i=0;i<h->ncmds;i++){ struct load_command*lc=(struct load_command*)p;
    if(lc->cmd==LC_ID_DYLIB){ struct dylib_command*d=(struct dylib_command*)p; id=(const char*)((unsigned long)d+d->nameoff); }
    if(lc->cmd==LC_DYLD_EXPORTS_TRIE){ struct linkedit_data_command*d=(struct linkedit_data_command*)p; *trie_off=d->dataoff; *trie_sz=d->datasize; }
    if(lc->cmd==LC_DYLD_INFO_ONLY){ struct dyld_info_command*d=(struct dyld_info_command*)p; *trie_off=d->export_off; *trie_sz=d->export_size; }
    if(lc->cmdsize<8) break; p+=lc->cmdsize; }
  return id;
}
static unsigned long uleb(const unsigned char**pp){ unsigned long r=0; int s=0; const unsigned char*p=*pp;
  for(int i=0;i<10;i++){ unsigned char b=*p++; r |= (unsigned long)(b&0x7f)<<s; if(!(b&0x80)) break; s+=7; } *pp=p; return r; }
// Walk export trie. link_edit_base = address (in memory) of the trie's first byte.
// Returns symbol address, searching the whole trie.
static unsigned long trie_lookup(unsigned long trie, const char*want, int wantlen){
  const unsigned char*p=(const unsigned char*)trie;
  // terminalSize at start of root node
  uleb(&p);
  // BFS/DFS over children recursively
  struct Frame { const unsigned char*p; int depth; } st[128];
  int sp=0; st[sp].p=(const unsigned char*)trie; st[sp].depth=0; sp++;
  while(sp){
    sp--; const unsigned char*q=st[sp].p; int depth=st[sp].depth;
    unsigned long terminal=uleb(&q);
    if(terminal && depth==wantlen){
      // verify prefix matches: we store prefix in the frame? simpler: we
      // only accept if caller checks separately. skip for now.
    }
    if(terminal){ /* record */ (void)q; }
    unsigned long nchild=*q++;
    for(unsigned long i=0;i<nchild;i++){
      const unsigned char*edge=q;
      const unsigned char*s=edge; while(*s) s++; unsigned char e=*s; s++;
      unsigned long childoff=uleb(&s);
      if(q-s < 0){}
      if(sp<128){ 
        // reconstruct prefix for matching: we need the accumulated string;
        // handled by an outer prefix buffer, see below
      }
      // advance to next child edge
      q = s;
    }
  }
  return 0;
}
// Simpler + correct: recursive search with explicit prefix buffer.
static unsigned long g_trie_base;
static int g_found; static unsigned long g_addr;
static void walk(const unsigned char*node, char*prefix, int plen, const char*want, int wlen){
  if(g_found) return;
  if(plen>200) return;
  const unsigned char*p=node;
  unsigned long terminal=uleb(&p);
  if(terminal){
    // skip to end of terminal payload
    const unsigned char*t=p; uleb(&t); // flags
    if(terminal>=3) { /* could be reexport/resolver; not for us */ }
    if(plen==wlen){
      int ok=1; for(int i=0;i<wlen;i++) if(prefix[i]!=want[i]){ok=0;break;}
      if(ok){
        // decode address: flags then address (if flags&0x08 -> reexport)
        const unsigned char*a=p; unsigned long flags=uleb(&a);
        if(!(flags&0x08)){ unsigned long off=uleb(&a); g_found=1; g_addr=off; }
      }
    }
  }
  unsigned long nchild=*p++;
  for(unsigned long i=0;i<nchild;i++){
    const unsigned char*s=p; while(*s) s++;
    unsigned long edge_len=(unsigned long)(s-p); s++;
    unsigned long childoff=uleb(&s);
    if(plen+(int)edge_len<400){
      for(unsigned long k=0;k<edge_len;k++) prefix[plen+k]=(char)p[k];
      walk((const unsigned char*)(g_trie_base+childoff), prefix, plen+(int)edge_len, want, wlen);
    }
    p=s;
  }
}
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
int main(int argc,char**argv,char**envp){
  int found=0;
  for(unsigned long a=0x180000000UL; a<0x200000000UL && found<4000; a+=0x4000){
    char vec[2]; vec[0]=0;
    if(rs3(78,a,16384,(long)vec)!=0) continue;
    if(((unsigned char)vec[0])&0x80) continue;
    if(!is_macho(a)) continue;
    found++;
    unsigned long to=0,ts=0; const char*id=get_id(a,&to,&ts);
    unsigned long tv=text_vmaddr(a); unsigned long slide=a-tv;
    if(!tv) continue;
    // trie bytes live at (unsild __LINKEDIT file offset -> mapped addr).
    // For the shared cache, LINKEDIT is mapped at slide + fileoff-like addr;
    // empirically export_off is a cache-absolute vmaddr minus slide.
    unsigned long trie=(to)+slide;
    // sanity: root node terminal size must be small
    if(rs3(78,trie,16384,(long)vec)==0 && !(((unsigned char)vec[0])&0x80)){
      const unsigned char*t=(const unsigned char*)trie;
      unsigned long term=0; { const unsigned char*p=t; term=uleb(&p); }
      if(term>1000) { trie = a + to; }
    }
    // Try to find getpid in this image
    char pref[400]; g_found=0; g_addr=0; g_trie_base=trie;
    walk((const unsigned char*)trie, pref, 0, "getpid", 6);
    if(g_found){ o_lbl("image "); o_puth(a); o_lbl(" "); o_puts(id); o_lbl(" getpid trie_off="); o_puth(g_addr); o_lbl(" => "); o_puth(trie+g_addr); o_lbl(" (trie="); o_puth(trie); o_lbl(")\n"); }
  }
  o_lbl("scanned "); o_putn(found); o_lbl(" images\n");
  return 0;
}
