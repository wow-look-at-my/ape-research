#include <stdio.h>
#include <stdint.h>
#include <string.h>
extern int cacheInit(void);
extern uint64_t resolveByContentEx(const char*,char*,size_t);
int main(void){ cacheInit();
  char p[256];
  const char *syms[]={"this_symbol_does_not_exist","another_bogus_xyz",NULL};
  for(int i=0;syms[i];i++){ uint64_t a=resolveByContentEx(syms[i],p,sizeof p);
    printf("  %-30s -> 0x%llx %s\n",syms[i],(unsigned long long)a, a?"UNEXPECTED":"cleanly not found"); }
  return 0; }
