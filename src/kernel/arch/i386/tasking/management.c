#include <cpu-state.h>
#include <kmalloc.h>
#include <process.h>
#include <stdarg.h>
#include <stdint.h>
#include <x86-segments.h>


#define KERNEL_STACK_SIZE 8192


void alloc_cpu_state(process_t *proc)
{
    proc->arch.kernel_stack_top = (uintptr_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;

    proc->cpu_state = (struct cpu_state *)proc->arch.kernel_stack_top - 1;
}


void initialize_cpu_state(struct cpu_state *state, void (*entry)(void), void *stack, int parcount, ...)
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

    state->eflags = 0x202;
}


void destroy_process_arch_struct(process_t *proc)
{
    kfree((void *)(proc->arch.kernel_stack_top - KERNEL_STACK_SIZE));
}
