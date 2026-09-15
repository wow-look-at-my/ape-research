#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
int main(void){
  uint64_t hits[]={0x188a39022ULL,0x18cd01780ULL,0x18d1b2092ULL,0x18d1c665fULL,0};
  for(int i=0;hits[i];i++){
    unsigned char*p=(unsigned char*)hits[i];
    printf("=== %#llx ===\n", (unsigned long long)hits[i]);
    printf("  ascii: "); for(int k=0;k<16;k++){ char c=p[k]; putchar((c>=32&&c<127)?c:'.'); } putchar('\n');
    printf("  hex:   "); for(int k=0;k<24;k++) printf("%02x ",p[k]); putchar('\n');
    printf("  ascii+64:"); for(int k=64;k<88;k++){ char c=p[k-64]; } 
    printf("  64+off ascii: "); for(int k=0;k<24;k++){ char c=p[k]; putchar((c>=32&&c<127)?c:'.'); } putchar('\n');
    // what precedes?
    printf("  before: "); for(int k=1;k<=8;k++) printf("%02x ",p[-k]); putchar('\n');
    fflush(stdout);
  }
  return 0;
}
