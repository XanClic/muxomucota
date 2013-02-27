#ifndef _SHM_H
#define _SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <vmem.h>


uintptr_t shm_create(size_t size);
uintptr_t shm_make(int count, void **vaddr_list, int *page_count_list, ptrdiff_t offset);
void *shm_open(uintptr_t shmid, unsigned flags);
void shm_close(uintptr_t shmid, const void *ptr, bool destroy);
void shm_unmake(uintptr_t shmid);

size_t shm_size(uintptr_t shmid);

#endif
