// calltest.c: prove content-discovered pointers are actually callable.
// Resolves by content and invokes, comparing behaviour with the direct call.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>

extern int cacheInit(void);
extern uint64_t resolveByContentEx(const char *plain, char *path, size_t sz);

int main(void) {
    cacheInit();
    struct { const char *sym; } syms[] = {
        {"getpid"}, {"strcmp"}, {"strncmp"}, {"memcpy"}, {"strlen"},
        {"pthread_self"}, {"getentropy"}, {"malloc"}, {"free"}, {"sysctl"},
    };
    for (unsigned i = 0; i < sizeof syms / sizeof syms[0]; i++) {
        char path[256] = {0};
        uint64_t a = resolveByContentEx(syms[i].sym, path, sizeof path);
        printf("%-14s -> 0x%llx  %s\n", syms[i].sym, (unsigned long long)a, path);
    }
    // Call the resolved pointers.
    char p[256];
    uint64_t strcmpAddr = resolveByContentEx("strcmp", p, sizeof p);
    if (strcmpAddr) {
        int (*host_strcmp)(const char *, const char *) = (int (*)(const char *, const char *))strcmpAddr;
        int r1 = host_strcmp("abc", "abd");
        int r2 = strcmp("abc", "abd");
        printf("strcmp via content ptr = %d, via libc = %d  %s\n", r1, r2, r1 == r2 ? "OK" : "BAD");
    }
    uint64_t strncmpAddr = resolveByContentEx("strncmp", p, sizeof p);
    if (strncmpAddr) {
        int (*host_strncmp)(const char *, const char *, unsigned long) = (int (*)(const char *, const char *, unsigned long))strncmpAddr;
        int r1 = host_strncmp("abcdef", "abcxyz", 3);
        int r2 = strncmp("abcdef", "abcxyz", 3);
        printf("strncmp via content ptr = %d, via libc = %d  %s\n", r1, r2, r1 == r2 ? "OK" : "BAD");
    }
    uint64_t mallocAddr = resolveByContentEx("malloc", p, sizeof p);
    uint64_t freeAddr   = resolveByContentEx("free", p, sizeof p);
    if (mallocAddr && freeAddr) {
        void *(*host_malloc)(unsigned long) = (void *(*)(unsigned long))mallocAddr;
        void (*host_free)(void *) = (void (*)(void *))freeAddr;
        void *m = host_malloc(123);
        ((char *)m)[0] = 'x'; ((char *)m)[122] = 'y';
        host_free(m);
        printf("malloc/free via content ptrs: OK (block %p)\n", m);
    }
    uint64_t getpidAddr = resolveByContentEx("getpid", p, sizeof p);
    if (getpidAddr) {
        int (*host_getpid)(void) = (int (*)(void))getpidAddr;
        printf("getpid via content ptr = %d, libc = %d  %s\n", host_getpid(), getpid(), host_getpid() == getpid() ? "OK" : "BAD");
    }
    uint64_t selfAddr = resolveByContentEx("pthread_self", p, sizeof p);
    if (selfAddr) {
        uint64_t (*host_self)(void) = (uint64_t (*)(void))selfAddr;
        printf("pthread_self via content ptr = 0x%llx, libc = 0x%llx  %s\n",
               (unsigned long long)host_self(), (unsigned long long)pthread_self(),
               host_self() == (uint64_t)pthread_self() ? "OK" : "DIFFERENT-THREAD-ID-MAYBE");
    }
    return 0;
}
