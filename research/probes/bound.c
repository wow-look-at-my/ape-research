#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
extern kern_return_t mach_vm_region(vm_map_t,mach_vm_address_t*,mach_vm_size_t*,vm_region_flavor_t,vm_region_info_t,mach_msg_type_number_t*,mach_port_t*);
static uint32_t r32(const unsigned char*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
int main(void){
	uint64_t cache=0;
	{register long x0 __asm__("x0")=(long)&cache;register long x16 __asm__("x16")=0x2000000|294;
	 __asm__ volatile("svc #0x80":"+r"(x0):"r"(x16):"x1","x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x17","memory","cc");}
	printf("cache=%p\n",(void*)cache);
	/* How far does the contiguous mapped region extend? */
	mach_vm_address_t a=cache; mach_vm_size_t sz=0;
	vm_region_basic_info_data_64_t ri; mach_msg_type_number_t c=VM_REGION_BASIC_INFO_COUNT_64; mach_port_t o=0;
	kern_return_t kr=mach_vm_region(mach_task_self(),&a,&sz,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&ri,&c,&o);
	printf("first region: addr=%p size=0x%llx prot=%d kr=%d\n",(void*)a,(unsigned long long)sz,ri.protection,kr);
	printf("=> the cache mapping is 0x%llx bytes; scan that far, not the header's mapping table\n",(unsigned long long)sz);
	/* Also: subCacheArrayCount / cacheType fields near 0x130 */
	const unsigned char*b=(const unsigned char*)cache;
	for(int off=0x120;off<0x180;off+=4) printf("  +0x%03x=%u\n",off,r32(b+off));
	return 0;
}
