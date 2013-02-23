#include <compiler.h>
#include <kassert.h>
#include <lock.h>
#include <process.h>
#include <vmem.h>


extern process_t *idle_process;

static lock_t dispatch_lock = LOCK_INITIALIZER;


struct cpu_state *dispatch(struct cpu_state *state, pid_t switch_to)
{
    if (unlikely(!trylock(&dispatch_lock)))
        return state;

    if (likely(current_process != NULL))
    {
        // Eine lustige Konstellation, die dazu führen könnte, dass zwei Prozesse den gleichen Kernelthread haben. Besser nicht ausprobieren.
        if (((uintptr_t)state < current_process->arch.kernel_stack) || ((uintptr_t)state >= current_process->arch.kernel_stack_top))
        {
            unlock(&dispatch_lock);
            return state;
        }

        current_process->cpu_state = state;
    }
    else if (likely(idle_process != NULL))
        idle_process->cpu_state = state;


    process_t *next_process = schedule(switch_to);

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
