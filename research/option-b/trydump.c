#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mapped(uint64_t a){ a&=~0x3fffULL; char v[2]; v[0]=0; if(syscall(SYS_mincore,(void*)a,(size_t)16384,v)!=0) return 0; return !((unsigned char)v[0]&0x80); }
static void dump(uint64_t a){ printf("%#llx mapped=%d: ",(unsigned long long)a,mapped(a));
  if(mapped(a)){ unsigned char*b=(unsigned char*)a; for(int i=0;i<24;i++) printf("%02x ",b[i]); printf(" | "); 
    for(int i=0;i<24;i++){char c=b[i];putchar((c>=32&&c<127)?c:'.');} }
  printf("\n"); fflush(stdout); }
int main(void){
  uint64_t slide=0x8830000ULL;
  // LINKEDIT subcache: unslid vm 0x1ff05c000, fileOff 0x4000
  uint64_t ld_unslid=0x1ff05c000ULL, ldfo=0x4000ULL;
  uint64_t trie_slid = ld_unslid+slide;
  printf("ld slid base=%#llx\n",(unsigned long long)trie_slid);
  uint64_t offs[]={0xb686d0ULL,0xb3cce0ULL,0xb71498ULL,0xb4dc50ULL,0};
  for(int i=0;offs[i];i++){
    uint64_t t=trie_slid+(offs[i]-ldfo);
    printf("off=%#llx -> ",(unsigned long long)offs[i]); dump(t);
  }
  printf("--- also try slid + off ---\n");
  for(int i=0;offs[i];i++){
    uint64_t t=trie_slid+offs[i];
    printf("off=%#llx -> ",(unsigned long long)offs[i]); dump(t);
  }
  printf("--- raw scan for 'dlsym' as substring anywhere ---\n");
  int hits=0;
  for(uint64_t a=0x180000000ULL;a<0x260000000ULL && hits<10;a+=0x4000){
    if(!mapped(a)) continue;
    unsigned char*pg=(unsigned char*)a;
    for(int j=0;j<16384-5;j++) if(pg[j]=='d'&&pg[j+1]=='l'&&pg[j+2]=='s'&&pg[j+3]=='y'&&pg[j+4]=='m'){
      printf("  dlsym @ %#llx prev=%02x\n",(unsigned long long)(a+j), pg[j-1]); hits++; if(hits>=10)break; }
    if(hits>=10)break;
  }
  return 0;
}
