#include <cpu-state.h>
#include <kassert.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <vmem.h>


bool handle_pagefault(struct cpu_state *state);

bool handle_pagefault(struct cpu_state *state)
{
    kassert(current_process != NULL);

    uintptr_t cr2;

    __asm__ __volatile__ ("mov %0,%%cr2" : "=r"(cr2));

    // Schreibzugriff, könnte COW sein
    if ((state->err_code & (1 << 1)) && vmmc_do_cow(current_process->vmmc, (void *)cr2))
        return true;

    return vmmc_do_lazy_map(current_process->vmmc, (void *)cr2);
}
