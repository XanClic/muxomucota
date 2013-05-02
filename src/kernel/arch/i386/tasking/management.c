/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <cpu-state.h>
#include <kassert.h>
#include <kmalloc.h>
#include <process.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <x86-segments.h>


#define KERNEL_STACK_SIZE 2048


void alloc_cpu_state_on_stack(process_t *proc, void *stack, size_t stacksz)
{
    proc->arch.kernel_stack     = (uintptr_t)stack;
    proc->arch.kernel_stack_top = proc->arch.kernel_stack + stacksz;

    proc->cpu_state = (struct cpu_state *)proc->arch.kernel_stack_top - 1;
}


void alloc_cpu_state(process_t *proc)
{
    alloc_cpu_state_on_stack(proc, kmalloc(KERNEL_STACK_SIZE), KERNEL_STACK_SIZE);
}


void initialize_cpu_state(process_t *proc, struct cpu_state *state, void (*entry)(void), void *stack, int parcount, ...)
{
    va_list va;

    va_start(va, parcount);

    state->eax = (parcount > 0) ? va_arg(va, uintptr_t) : 0;
    state->ebx = (parcount > 1) ? va_arg(va, uintptr_t) : 0;
    state->ecx = (parcount > 2) ? va_arg(va, uintptr_t) : 0;
    state->edx = (parcount > 3) ? va_arg(va, uintptr_t) : 0;
    state->esi = (parcount > 4) ? va_arg(va, uintptr_t) : 0;
    state->edi = (parcount > 5) ? va_arg(va, uintptr_t) : 0;
    state->ebp = (parcount > 6) ? va_arg(va, uintptr_t) : 0;

    va_end(va);


    state->cs  = SEG_USR_CS;
    state->eip = (uintptr_t)entry;

    state->ds  = SEG_USR_DS;
    state->es  = SEG_USR_DS;

    state->ss  = SEG_USR_DS;
    state->esp = (uintptr_t)stack;

    state->eflags = 0x202 | (proc->arch.iopl << 12);
}


void initialize_kernel_thread_cpu_state(process_t *proc, struct cpu_state *state, void (*entry)(void))
{
    state->eax = state->ebx = state->ecx = state->edx = state->esi = state->edi = state->ebp = 0;

    state->cs  = SEG_SYS_CS;
    state->eip = (uintptr_t)entry;

    state->ds  = SEG_SYS_DS;
    state->es  = SEG_SYS_DS;

    state->eflags = 0x202 | (proc->arch.iopl << 12);
}


void initialize_forked_cpu_state(struct cpu_state *dest, const struct cpu_state *src)
{
    memcpy(dest, src, sizeof(*src));

    dest->eax = 0;
}


void initialize_child_process_arch(process_t *child, process_t *parent)
{
    child->arch.iopl = parent->arch.iopl;

    // an 16 B ausrichten
    child->arch.fxsave = (struct fxsave_space *)(((uintptr_t)child->arch.fxsave_real_space + 0xf) & ~0xf);
    child->arch.fxsave_valid = false;
}

void initialize_orphan_process_arch(process_t *proc)
{
    proc->arch.iopl = 0;

    proc->arch.fxsave = (struct fxsave_space *)(((uintptr_t)proc->arch.fxsave_real_space + 0xf) & ~0xf);
    proc->arch.fxsave_valid = false;
}


void *process_stack_alloc(struct cpu_state *state, size_t size)
{
    state->esp -= size;

    return (void *)state->esp;
}


void add_process_func_param(process_t *proc, struct cpu_state *state, uintptr_t param)
{
    void *base = process_stack_alloc(state, sizeof(param));

    vmmc_do_lazy_map(proc->vmmc, base);

    uintptr_t paddr;
    kassert_exec(vmmc_address_mapped(proc->vmmc, base, &paddr));

    uintptr_t *mapped = kernel_map(paddr, sizeof(param));

    *mapped = param;

    kernel_unmap(mapped, sizeof(param));
}


void set_process_func_regparam(struct cpu_state *state, uintptr_t param)
{
    state->eax = param;
}


void process_simulate_func_call(struct cpu_state *state)
{
    state->esp -= sizeof(void (*)(void));
}


void process_set_initial_params(process_t *proc, struct cpu_state *state, int argc, const char *const *argv, const char *const *envp)
{
    uintptr_t initial = proc->cpu_state->esp;

    // GCC möchte den Stack an 16 ausgerichtet haben.
    state->esp = (state->esp - sizeof(argc) - sizeof(argv) - sizeof(envp)) & ~0xF;


    uintptr_t end = (initial + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uintptr_t stack_page = proc->cpu_state->esp & ~(PAGE_SIZE - 1); stack_page < end; stack_page += PAGE_SIZE)
        vmmc_do_lazy_map(proc->vmmc, (void *)stack_page);


    uintptr_t paddr;
    kassert_exec(vmmc_address_mapped(proc->vmmc, (void *)state->esp, &paddr));

    // Das muss sich innerhalb einer Page befinden (insg. acht Byte, an 16
    // ausgerichtet).
    void *mapped_stack = kernel_map(paddr, sizeof(argc) + sizeof(argv) + sizeof(envp));

    *(int *)mapped_stack = argc;
    ((const char *const **)((int *)mapped_stack + 1))[0] = argv;
    ((const char *const **)((int *)mapped_stack + 1))[1] = envp;

    kernel_unmap(mapped_stack, sizeof(argc) + sizeof(argv) + sizeof(envp));

    process_simulate_func_call(state);
}


void destroy_process_arch_struct(process_t *proc)
{
    kfree((void *)proc->arch.kernel_stack);

    fpu_sse_unregister(proc);
}


void kiopl(int level, struct cpu_state *state)
{
    current_process->arch.iopl = level;

    state->eflags = (state->eflags & ~(3 << 12)) | (level << 12);
}
