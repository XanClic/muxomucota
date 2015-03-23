#include <cpu-state.hpp>
#include <cstdint>
#include <kassert.hpp>
#include <process.hpp>
#include <vmem.hpp>


bool handle_pagefault(cpu_state *state);

bool handle_pagefault(cpu_state *state)
{
    if (!current_process) {
        return false;
    }

    void *cr2;
    __asm__ __volatile__ ("mov %%cr2,%0" : "=r"(cr2));

    // write access, might be COW
    if ((state->err_code & (1 << 1)) && current_process->vmm->do_cow(cr2)) {
        return true;
    }

    return current_process->vmm->do_lazy_map(cr2);
}
