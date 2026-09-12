/* Confirm the compiled Syslib struct offsets match cosmo's expectations by
   printing offsets the way the runtime would read them. */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
struct Syslib {
	int magic; int version;
	void *fork; void *pipe; void *clock_gettime; void *nanosleep; void *mmap;
	void *p1; void *p2; void *p3; void *p4; void *p5; void *p6; void *p7; void *p8;
	void *d1; void *d2; void *d3; void *d4;
	void *pself; void *drel; void *raise; void *pjoin; void *pyield;
	int pstackmin; int sizeattr;
	void *a1; void *a2; void *a3; void *a4;
	void *exit_; void *close_; void *munmap_; void *openat_; void *write_; void *read_;
	void *sigaction_; void *pselect_; void *mprotect_;
	void *sigaltstack_; void *getentropy_;
	void *s1; void *s2; void *s3; void *s4; void *s5; void *s6; void *gr; void *sr;
	void *dlopen_; void *dlsym_; void *dlclose_; void *dlerror_;
	void *cpu; void *sysctl_; void *sysctlbyname_; void *sysctlnametomib_;
};
int main(void){
	printf("sizeof(struct Syslib) = %zu\n", sizeof(struct Syslib));
	printf("offsetof magic=%zu version=%zu\n", offsetof(struct Syslib,magic), offsetof(struct Syslib,version));
	printf("offsetof mmap=%zu\n", offsetof(struct Syslib,mmap));
	printf("offsetof pthread_self=%zu\n", offsetof(struct Syslib,pself));
	printf("offsetof exit=%zu\n", offsetof(struct Syslib,exit_));
	printf("offsetof getentropy=%zu\n", offsetof(struct Syslib,getentropy_));
	printf("offsetof dlopen=%zu\n", offsetof(struct Syslib,dlopen_));
	printf("offsetof sysctlnametomib=%zu\n", offsetof(struct Syslib,sysctlnametomib_));
	return 0;
}
