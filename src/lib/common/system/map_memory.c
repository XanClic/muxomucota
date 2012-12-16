#include <drivers/memory.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


void *map_memory(uintptr_t physical, size_t length, unsigned flags)
{
    return (void *)syscall3(SYS_MAP_MEMORY, physical, length, flags);
}
