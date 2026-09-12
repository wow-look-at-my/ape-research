#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/task_info.h>

int main(void) {
	struct task_dyld_info tdi; mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;
	kern_return_t kr = task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&tdi, &cnt);
	printf("task_info kr=%d all_image_info_addr=%p size=%llu\n",
	       kr, (void*)tdi.all_image_info_addr, tdi.all_image_info_size);
	uintptr_t target = tdi.all_image_info_addr;
	unsigned char *cb = (unsigned char *)0x0000000FFFFFC000ULL;
	printf("comm signature: %.16s\n", cb);
	for (long off = 0; off < 0x4000; off += 8) {
		uintptr_t v; memcpy(&v, cb+off, 8);
		if (v == target) printf("*** dyld_all_image_infos at comm offset 0x%03lx\n", off);
	}
	/* sanity: dump version/count at target */
	uint32_t ver, n; memcpy(&ver, (void*)target, 4); memcpy(&n, (void*)(target+4), 4);
	printf("target: version=%u infoArrayCount=%u\n", ver, n);
	return 0;
}
