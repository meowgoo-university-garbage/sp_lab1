#include <stdio.h>
#include <sys/mman.h>

#include "macros.h"
#include "kernel.h"

void *meowlloc_kernel_acquireMemory(size_t size) {
    char *arena = mmap(null, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return arena;
}

void meowlloc_kernel_unuseMemory(void *start, size_t size) {
    madvise(start, size, MADV_FREE);
    // printf("MADVISED: %p %ld\n", start, size);
}

void meowlloc_kernel_releaseMemory(void *start, size_t size) {
    munmap(start, size);
}
