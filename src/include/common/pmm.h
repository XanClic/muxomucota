#ifndef _PMM_H
#define _PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void init_pmm(void);

uintptr_t pmm_alloc(void);
uintptr_t pmm_alloc_advanced(uintptr_t max_addr, size_t length, int alignment);
void pmm_free(uintptr_t paddr);

void pmm_use(uintptr_t paddr);

int pmm_user_count(uintptr_t paddr);
bool pmm_alloced(uintptr_t paddr);

void pmm_mark_cow(uintptr_t paddr, bool flag);
bool pmm_is_cow(uintptr_t paddr);

#endif
