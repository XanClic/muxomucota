#include <cpu-state.h>
#include <kassert.h>
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


void *process_stack_alloc(struct cpu_state *state, size_t size)
{
    state->esp -= size;

    return (void *)state->esp;
}


void process_set_initial_params(process_t *proc, int argc, const char *const *argv)
{
    uintptr_t initial = proc->cpu_state->esp;

    // GCC möchte den Stack an 16 ausgerichtet haben.
    proc->cpu_state->esp = (proc->cpu_state->esp - sizeof(argc) - sizeof(argv)) & ~0xF;


    uintptr_t end = (initial + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uintptr_t stack_page = proc->cpu_state->esp & ~(PAGE_SIZE - 1); stack_page < end; stack_page += PAGE_SIZE)
        vmmc_do_lazy_map(proc->vmmc, (void *)stack_page);


    uintptr_t paddr;
    kassert_exec(vmmc_address_mapped(proc->vmmc, (void *)proc->cpu_state->esp, &paddr));

    // Das muss sich innerhalb einer Page befinden (insg. acht Byte, an 16
    // ausgerichtet).
    void *mapped_stack = kernel_map(paddr, sizeof(argc) + sizeof(argv));

    *(int *)mapped_stack = argc;
    *(const char *const **)((int *)mapped_stack + 1) = argv;

    kernel_unmap(mapped_stack, sizeof(argc) + sizeof(argv));

    // Ein Funktionsaufruf, ein simulierter. Was denn sonst?
    proc->cpu_state->esp -= sizeof(void (*)(void));
}


void destroy_process_arch_struct(process_t *proc)
{
    kfree((void *)(proc->arch.kernel_stack_top - KERNEL_STACK_SIZE));
}


void initialize_fork_cpu_state_from_syscall_stack(struct cpu_state *state, void *stack)
{
    uint32_t *ss = stack;

    state->eax = 0;
    state->ebx = ss[1];
    state->ecx = ss[2];
    state->edx = ss[3];
    state->esi = ss[4];
    state->edi = ss[5];
    state->ebp = ss[6];


    state->cs  = SEG_USR_CS;
    state->eip = ss[9];

    state->ds  = SEG_USR_DS;
    state->es  = SEG_USR_DS;

    state->ss  = SEG_USR_DS;
    state->esp = ss[12];

    state->eflags = ss[11];
}
