#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <mach/mach.h>
int main(void){ printf("mach_task_self() = %d\n", mach_task_self()); return 0; }
