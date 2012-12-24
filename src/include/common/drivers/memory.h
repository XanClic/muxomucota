#ifndef _DRIVERS__MEMORY_H
#define _DRIVERS__MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <vmem.h>


void *map_memory(uintptr_t physical, size_t length, unsigned flags);
void unmap_memory(const void *ptr, size_t length);

#endif
