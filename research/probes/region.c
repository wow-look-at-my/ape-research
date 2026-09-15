#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <signal.h>
#include <setjmp.h>

/* libSystem exports this MIG stub; the SDK header lacks a declaration. */
extern kern_return_t mach_vm_region(vm_map_t, mach_vm_address_t *, mach_vm_size_t *,
                                    vm_region_flavor_t, vm_region_info_t,
                                    mach_msg_type_number_t *, mach_port_t *);

static sigjmp_buf jb;
static void h(int s){ (void)s; siglongjmp(jb,1); }

int main(void) {
	signal(SIGBUS, h); signal(SIGSEGV, h);
	mach_vm_address_t a = 0x0FFFFC000ULL; mach_vm_size_t sz=0;
	vm_region_basic_info_data_64_t ri; mach_msg_type_number_t c = VM_REGION_BASIC_INFO_COUNT_64;
	mach_port_t obj=0;
	kern_return_t kr = mach_vm_region(mach_task_self(), &a, &sz, VM_REGION_BASIC_INFO_64,
	                                  (vm_region_info_t)&ri, &c, &obj);
	printf("vm_region kr=%d addr=0x%llx size=0x%llx prot=%d\n", kr, a, sz, ri.protection);
	for (long off = 0; off < 0x4000; off += 8) {
		if (sigsetjmp(jb,1)) { printf("  BUS at 0x%lx -- stop\n", off); break; }
		uintptr_t v; memcpy(&v, (void*)(0x0FFFFC000ULL+off), 8);
		if (v > 0x100000000ULL && v < 0x0000FFFF00000000ULL && (v & 3) == 0)
			printf("  comm+0x%03lx = 0x%lx\n", off, v);
	}
	return 0;
}
