#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); if(!f){perror("open");return 1;}
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  unsigned char*b=malloc(n); fread(b,1,n,f); fclose(f);
  printf("file size %ld\n",n);
  for(long off=0x10000; off<n; off+=0x10000){
    if(!memcmp(b+off,"\177ELF",4)){
      uint16_t mach=*(uint16_t*)(b+off+18);
      printf("ELF @ %#lx machine=%u\n",off,mach);
      if(mach==183){
        uint64_t entry=*(uint64_t*)(b+off+24), phoff=*(uint64_t*)(b+off+32);
        uint16_t phnum=*(uint16_t*)(b+off+56), phentsize=*(uint16_t*)(b+off+54);
        printf("  ET=%u entry=%#lx phoff=%#lx phnum=%u phentsize=%u\n",*(uint16_t*)(b+off+16),entry,phoff,phnum,phentsize);
        for(int i=0;i<phnum;i++){
          unsigned char*p=b+off+phoff+i*phentsize;
          uint32_t type=*(uint32_t*)p, flags=*(uint32_t*)(p+4);
          uint64_t o=*(uint64_t*)(p+8), va=*(uint64_t*)(p+16), pa=*(uint64_t*)(p+24), fs=*(uint64_t*)(p+32), ms=*(uint64_t*)(p+40), al=*(uint64_t*)(p+48);
          printf("  ph[%d] type=%u flags=%u off=%#lx va=%#lx fs=%#lx ms=%#lx align=%#lx\n",i,type,flags,o,va,fs,ms,al);
        }
      }
    }
  }
  return 0;
}
