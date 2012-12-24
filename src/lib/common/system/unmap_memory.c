#include <drivers/memory.h>
#include <syscall.h>


void unmap_memory(const void *ptr, size_t length)
{
    syscall2(SYS_UNMAP_MEMORY, (uintptr_t)ptr, length);
}
