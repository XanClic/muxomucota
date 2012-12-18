#ifndef _PMM_H
#define _PMM_H

#include <stdint.h>


void init_pmm(void);

uintptr_t pmm_alloc(int count);
void pmm_free(uintptr_t start, int count);

void pmm_use(uintptr_t start, int count);

#endif
