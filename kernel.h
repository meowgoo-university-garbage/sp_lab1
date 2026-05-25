#ifndef MEOWLLOC_KERNEL_H
#define MEOWLLOC_KERNEL_H

#define MEOWLLOC_KERNEL_ERROR ((void *)(-1))

void *meowlloc_kernel_acquireMemory(size_t size);
void meowlloc_kernel_unuseMemory(void *start, size_t size);
void meowlloc_kernel_releaseMemory(void *start, size_t size);

#endif // MEOWLLOC_KERNEL_H
