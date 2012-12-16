#include <compiler.h>
#include <process.h>
#include <vmem.h>


extern process_t *idle_process;


struct cpu_state *dispatch(struct cpu_state *state)
{
    if (likely(current_process != NULL))
        current_process->cpu_state = state;
    else if (likely(idle_process != NULL))
        idle_process->cpu_state = state;

    process_t *next_process = schedule();

    if (unlikely(next_process == NULL))
        return state;

    change_vmm_context(next_process->vmmc);

    arch_process_change(next_process);

    return next_process->cpu_state;
}
