#include <shm.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>


size_t shm_size(uintptr_t shmid)
{
    return syscall1(SYS_SHM_SIZE, shmid);
}
