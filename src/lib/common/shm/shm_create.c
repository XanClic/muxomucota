#include <shm.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


uintptr_t shm_create(size_t size)
{
    return syscall1(SYS_SHM_CREATE, size);
}
