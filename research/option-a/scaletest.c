// scaletest.c: resolve a large symbol list by content and compare against dlsym.
// Reports MATCH / DIFF-STUB / NOT-FOUND so the resolver's real coverage is known.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/syscall.h>

// Reuse the resolver from cacheresolve.c by compiling both and calling its globals.
extern int cacheInit(void);
extern uint64_t resolveByContentEx(const char *plain, char *path, size_t sz);
extern void cacheInfo(uint64_t *base, int64_t *slide, uint32_t *n);

int main(int argc, char **argv) {
    cacheInit();
    uint64_t base; int64_t slide; uint32_t n;
    cacheInfo(&base, &slide, &n);
    printf("cacheBase=0x%llx slide=0x%llx images=%u\n", (unsigned long long)base, (unsigned long long)slide, n);

    // Build the symbol list: Syslib contract + a broad set of libc symbols.
    const char *syslib[] = {
        "fork","pipe","clock_gettime","nanosleep","mmap","pthread_jit_write_protect_supported_np",
        "pthread_jit_write_protect_np","sys_icache_invalidate","pthread_create","pthread_exit",
        "pthread_kill","pthread_sigmask","pthread_setname_np","dispatch_semaphore_create",
        "dispatch_semaphore_signal","dispatch_semaphore_wait","dispatch_walltime","pthread_self",
        "dispatch_release","raise","pthread_join","pthread_yield_np","pthread_attr_init",
        "pthread_attr_destroy","pthread_attr_setstacksize","pthread_attr_setguardsize","exit",
        "close","munmap","openat","write","read","sigaction","pselect","mprotect","sigaltstack",
        "getentropy","sem_open","sem_unlink","sem_close","sem_post","sem_wait","sem_trywait",
        "getrlimit","setrlimit","dlopen","dlsym","dlclose","dlerror","pthread_cpu_number_np",
        "sysctl","sysctlbyname","sysctlnametomib", NULL
    };
    const char *more[] = {
        "malloc","free","calloc","realloc","memcpy","memmove","memset","memcmp","strlen","strcmp",
        "strncmp","strcpy","strncpy","strcat","strstr","strchr","printf","fprintf","sprintf",
        "snprintf","vsnprintf","puts","fputs","fwrite","fread","fopen","fclose","open","lseek",
        "stat","fstat","readlink","getcwd","chdir","access","unlink","rename","mkdir","rmdir",
        "getuid","geteuid","getgid","getegid","getpid","getppid","kill","wait4","gettimeofday",
        "clock_gettime","time","localtime","gmtime","mktime","strftime","qsort","bsearch","abs",
        "atoi","atol","strtol","strtoul","strtod","rand","srand","abort","atexit","getenv",
        "setenv","unsetenv","putenv","isatty","dup","dup2","fcntl","ioctl","select","poll",
        "socket","connect","bind","listen","accept","send","recv","shutdown","getaddrinfo",
        "freeaddrinfo","gethostbyname","pthread_mutex_init","pthread_mutex_lock","pthread_mutex_unlock",
        "pthread_mutex_destroy","pthread_cond_init","pthread_cond_wait","pthread_cond_signal",
        "pthread_cond_broadcast","pthread_cond_destroy","pthread_key_create","pthread_setspecific",
        "pthread_getspecific","pthread_once","pthread_detach","pthread_cancel","dladdr","dlinfo",
        "__error","__stack_chk_fail","__stack_chk_guard","_NSGetExecutablePath","_dyld_get_image_header",
        "os_unfair_lock_lock","os_unfair_lock_unlock","mach_absolute_time","mach_task_self_",
        "vm_allocate","vm_deallocate","sysconf","getpagesize","getrlimit","setrlimit",
        "__cxa_atexit","__cxa_finalize","_exit","_Exit","strerror","perror","srandom",
        "arc4random","arc4random_buf","getentropy","sysctlbyname","posix_spawn","execve","execvp",
        NULL
    };
    int match = 0, diff = 0, notfound = 0, total = 0, ref0 = 0;
    for (int pass = 0; pass < 2; pass++) {
        const char **list = pass ? more : syslib;
        for (int i = 0; list[i]; i++) {
            const char *s = list[i];
            // skip duplicates in the second pass
            if (pass) {
                int dup = 0;
                for (int k = 0; k < i; k++) if (!strcmp(more[k], s)) { dup = 1; break; }
                if (dup) continue;
            }
            char path[256]; path[0] = 0;
            uint64_t a = resolveByContentEx(s, path, sizeof path);
            void *ref = dlsym(RTLD_DEFAULT, s);
            total++;
            if (!a) { notfound++; printf("NOTFOUND  %-30s\n", s); continue; }
            if (!ref) { ref0++; printf("NODLSYM   %-30s cache=0x%llx (%s)\n", s, (unsigned long long)a, path); continue; }
            if (a == (uint64_t)ref) { match++; }
            else { diff++; printf("DIFF      %-30s cache=0x%llx dlsym=0x%llx (%s)\n", s, (unsigned long long)a, (unsigned long long)(uint64_t)ref, path); }
        }
    }
    printf("\ntotal=%d MATCH=%d DIFF=%d NOTFOUND=%d (dlsym==0: %d)\n", total, match, diff, notfound, ref0);
    return 0;
}
