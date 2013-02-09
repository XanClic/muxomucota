#ifndef _PMM_H
#define _PMM_H

#include <stdbool.h>
#include <stdint.h>


void init_pmm(void);

uintptr_t pmm_alloc(void);
void pmm_free(uintptr_t paddr);

void pmm_use(uintptr_t paddr);

int pmm_user_count(uintptr_t paddr);
bool pmm_alloced(uintptr_t paddr);

void pmm_mark_cow(uintptr_t paddr, bool flag);
bool pmm_is_cow(uintptr_t paddr);

#endif
