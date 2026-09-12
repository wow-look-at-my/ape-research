#include "rs.h"
// usage: scancall N  -> child calls BSD syscall N with 0 args, prints rc, exits
int main(int argc,char**argv,char**envp){
  long n=0; { const char*s=argv[1]; while(*s){n=n*10+(*s-'0');s++;} }
  long r=rs0(n);
  o_lbl("rc="); o_putn(r); o_lbl("\n");
  return 0;
}
