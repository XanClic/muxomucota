#include <shm.h>
#include <stdint.h>
#include <syscall.h>


void shm_unmake(uintptr_t shmid, bool destroy)
{
    syscall2(SYS_SHM_UNMAKE, shmid, destroy);
}
