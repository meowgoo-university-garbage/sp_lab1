#ifndef __MEOWLLOC_H
#define __MEOWLLOC_H

#include "config.h"

void *meowlloc_allocate(size_t size);
void *meowlloc_reallocate(void *old, size_t newSize);
void meowlloc_free(void *old);

#endif // __MEOWLLOC_H
