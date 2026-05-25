#ifndef __MEOWLLOC_H
#define __MEOWLLOC_H

#include <stddef.h>

#define max_align_t int

#include "config.h"

#define MEOWLLOC_ALLOCATE(ty) meowlloc_allocate(sizeof(ty))
#define MEOWLLOC_ALLOCATE_N(ty, n) meowlloc_allocate(sizeof(ty) * (n))
#define MEOWLLOC_REALLOCATE_N(old, ty, n) meowlloc_reallocate((old), sizeof(ty) * (n))

void *meowlloc_allocate(size_t size);
void *meowlloc_reallocate(void *old, size_t newSize);
void meowlloc_free(void *old);

#endif // __MEOWLLOC_H
