#include <shm.h>
#include <stdint.h>
#include <syscall.h>


void *shm_open(uintptr_t shmid, unsigned flags)
{
    return (void *)syscall2(SYS_SHM_OPEN, shmid, flags);
}
