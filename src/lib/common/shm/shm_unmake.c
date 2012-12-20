#include <shm.h>
#include <stdint.h>
#include <syscall.h>


void shm_unmake(uintptr_t shmid)
{
    syscall1(SYS_SHM_UNMAKE, shmid);
}
