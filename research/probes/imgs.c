typedef long i64; typedef unsigned long long u64; typedef unsigned char u8; typedef unsigned int u32;
#include <mach-o/dyld.h>
int main(void){ return _dyld_image_count(); }   /* returns count as exit code */
