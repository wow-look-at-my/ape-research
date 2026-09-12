#include "rs.h"
static int mm(unsigned long a){ if(a&7)return 0; char v[2]; v[0]=0; if(rs3(78,a,16384,(long)v)!=0) return 0; return !(((unsigned char)v[0])&0x80); }
int main(int argc,char**argv,char**envp){
  unsigned long cache=0x188830000UL;
  // dump header qwords
  unsigned long*q=(unsigned long*)cache;
  o_lbl("cache header qwords:\n");
  for(int i=0;i<24;i++){ o_lbl("  +"); o_puth((unsigned long)(i*8)); o_lbl(" = "); o_puth(q[i]); o_lbl("\n"); }
  unsigned int mappingOffset=*(unsigned int*)(cache+0x10);
  unsigned int mappingCount=*(unsigned int*)(cache+0x14);
  unsigned long imagesTextOffset=*(unsigned long*)(cache+0x90);
  unsigned long imagesTextCount=*(unsigned long*)(cache+0x98);
  o_lbl("mappingOffset="); o_puth(mappingOffset); o_lbl(" mappingCount="); o_putn(mappingCount); o_lbl("\n");
  o_lbl("imagesTextOffset="); o_puth(imagesTextOffset); o_lbl(" imagesTextCount="); o_putn((long)imagesTextCount); o_lbl("\n");
  // mapping table: each entry 32 bytes {address, size, fileOffset, maxProt, initProt, ...}
  for(unsigned int i=0;i<mappingCount && i<16;i++){
    unsigned long*e=(unsigned long*)(cache+mappingOffset+i*32);
    o_lbl("  map["); o_putn(i); o_lbl("] addr="); o_puth(e[0]); o_lbl(" size="); o_puth(e[1]); o_lbl(" fileOff="); o_puth(e[2]); o_lbl("\n");
  }
  // Now test trie candidates for libSystem: trie_off=0x10e6488
  unsigned long to=0x10e6488UL;
  unsigned long slide=cache-0x180000000UL;
  unsigned long cands[6]={0x180000000UL+to+slide, 0x180000000UL+to, to+cache, to, 0x190340000UL+to, 0x1ff05c000UL+to};
  const char*nm[6]={"vm0+off+slide","vm0+off","cache+off","off","textvm+off","ldvm+off"};
  for(int i=0;i<6;i++){
    unsigned long c=cands[i];
    o_lbl("trie["); o_puts(nm[i]); o_lbl("]="); o_puth(c);
    if(!mm(c)){ o_lbl(" UNMAPPED\n"); continue; }
    unsigned char*b=(unsigned char*)c;
    o_lbl(" bytes="); for(int k=0;k<12;k++){ o_puth(b[k]); o_lbl(" "); } 
    o_lbl(" | "); for(int k=0;k<12;k++){ char ch=b[k]; o_write(1,(ch>=32&&ch<127)?&b[k]:".",1); }
    o_lbl("\n");
  }
  return 0;
}
