#include "rs.h"
int main(int argc,char**argv,char**envp){
  struct { unsigned long handler; unsigned int mask; int flags; } old;
  old.handler=0xdead;old.mask=0xdead;old.flags=0xdead;
  long r=rs3(46,10,0,(long)&old);
  o_lbl("query SIGBUS rc=");o_putn(r);o_lbl(" handler=");o_puth(old.handler);o_lbl(" mask=");o_puth(old.mask);o_lbl(" flags=");o_puth((unsigned)old.flags);o_lbl("\n");
  old.handler=0xdead;old.mask=0xdead;old.flags=0xdead;
  r=rs3(46,11,0,(long)&old);
  o_lbl("query SIGSEGV rc=");o_putn(r);o_lbl(" handler=");o_puth(old.handler);o_lbl(" mask=");o_puth(old.mask);o_lbl(" flags=");o_puth((unsigned)old.flags);o_lbl("\n");
  return 0;
}
