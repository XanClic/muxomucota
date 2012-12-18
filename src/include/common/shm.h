#ifndef _SHM_H
#define _SHM_H

#include <stddef.h>
#include <stdint.h>
#include <vmem.h>


uintptr_t shm_create(size_t size);
void *shm_open(uintptr_t shmid, unsigned flags);
void shm_close(uintptr_t shmid, void *ptr);

size_t shm_size(uintptr_t shmid);

#endif
