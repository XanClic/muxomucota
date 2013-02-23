#include <syscall.h>
#include <drivers/memory.h>


uintptr_t phys_alloc(size_t length, uintptr_t max_addr, int alignment)
{
    return syscall3(SYS_PHYS_ALLOC, length, max_addr, alignment);
}
