#ifndef _KMALLOC_H
#define _KMALLOC_H

#include <stddef.h>
#include <string.h>


void *kmalloc(size_t size);
void kfree(void *ptr);

static inline void *kcalloc(size_t size) { return memset(kmalloc(size), 0, size); }

#endif
