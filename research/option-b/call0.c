#include "rs.h"
#include <stddef.h>
int main(int argc,char**argv,char**envp){
  // Known dlsym address from a same-machine run. Test whether a zero-import
  // binary can call libSystem functions at hardcoded addresses.
  void*(*my_dlsym)(void*,const char*)=(void*(*)(void*,const char*))0x188910c04UL;
  o_lbl("calling dlsym at hardcoded addr...\n");
  void*p=my_dlsym((void*)-2,"getpid");
  o_lbl("getpid="); o_puth((unsigned long)p); o_lbl("\n");
  void*p2=my_dlsym((void*)-2,"malloc");
  o_lbl("malloc="); o_puth((unsigned long)p2); o_lbl("\n");
  if(p){ long (*gp)(void)=(long(*)(void))p; o_lbl("getpid()="); o_putn(gp()); o_lbl("\n"); }
  return 0;
}
