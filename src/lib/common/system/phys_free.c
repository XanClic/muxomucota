#include <syscall.h>
#include <drivers/memory.h>


void phys_free(uintptr_t address, size_t length)
{
    syscall2(SYS_PHYS_FREE, address, length);
}
