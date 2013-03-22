#ifndef _VM86_H
#define _VM86_H

#include <cpu-state.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


struct vm86_registers
{
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint32_t ds, es, fs, gs;
};

struct vm86_memory_area
{
    uintptr_t vm;
    void *caller;
    size_t size;
    bool in, out;
};


// Returns 0 on success, -errno on error.
void vm86_interrupt(int intr, struct vm86_registers *regs, struct vm86_memory_area *mem, int areas);

#ifdef KERNEL
bool handle_vm86_gpf(struct cpu_state *state);
#endif

#endif
