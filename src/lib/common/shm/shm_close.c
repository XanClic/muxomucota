#include <shm.h>
#include <stdint.h>
#include <syscall.h>


void shm_close(uintptr_t shmid, const void *ptr)
{
    syscall2(SYS_SHM_CLOSE, shmid, (uintptr_t)ptr);
}
