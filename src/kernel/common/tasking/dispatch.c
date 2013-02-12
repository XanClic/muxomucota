#include <compiler.h>
#include <lock.h>
#include <process.h>
#include <vmem.h>


extern process_t *idle_process;

static lock_t dispatch_lock = unlocked;


struct cpu_state *dispatch(struct cpu_state *state)
{
    if (unlikely(!trylock(&dispatch_lock)))
        return state;

    if (likely(current_process != NULL))
        current_process->cpu_state = state;
    else if (likely(idle_process != NULL))
        idle_process->cpu_state = state;


    process_t *next_process = schedule();

    if (unlikely(next_process == NULL))
    {
        unlock(&dispatch_lock);
        return state;
    }


    change_vmm_context(next_process->vmmc);

    arch_process_change(next_process);


    unlock(&dispatch_lock);

    return next_process->cpu_state;
}
