#include <compiler.h>
#include <kassert.h>
#include <lock.h>
#include <process.h>
#include <stddef.h>


lock_t schedule_lock = LOCK_INITIALIZER;

extern process_t *runqueue, *idle_process;


process_t *schedule(pid_t switch_to)
{
    static unsigned schedule_tick = 0;


    if (!trylock(&schedule_lock))
        return current_process;


    if (unlikely(current_process == NULL))
    {
        if (idle_process != NULL)
            current_process = idle_process;
        else
        {
            unlock(&schedule_lock);
            return NULL;
        }
    }


    current_process->schedule_tick = schedule_tick++;


    if ((current_process == idle_process) || (current_process->status != PROCESS_ACTIVE))
        current_process = runqueue;


    if (current_process == NULL)
    {
        unlock(&schedule_lock);
        return idle_process;
    }


    process_t *switch_target = NULL, *irq_process = NULL;

    unsigned max_sched_diff = 0;
    process_t *eldest = NULL;


    for (process_t *proc = runqueue; proc != NULL; proc = proc->rq_next)
    {
        if (proc->status != PROCESS_ACTIVE)
            continue;

        if (proc->pid == switch_to)
            switch_target = proc;

        if ((proc->handled_irq >= 0) && proc->handling_irq && proc->fresh_irq)
            irq_process = proc;

        if (schedule_tick - proc->schedule_tick > max_sched_diff)
        {
            max_sched_diff = schedule_tick - proc->schedule_tick;
            eldest = proc;
        }
    }


    if (eldest == NULL)
    {
        current_process = idle_process;

        unlock(&schedule_lock);

        return current_process;
    }


    current_process = eldest;

    if ((switch_target != NULL) && (max_sched_diff < 42))
        current_process = switch_target;

    if (irq_process != NULL)
    {
        irq_process->fresh_irq = false;
        current_process = irq_process;
    }


    unlock(&schedule_lock);

    if (current_process->pid != current_process->pgid)
        kassert((current_process->cpu_state->esp < 0xef000000) || !(current_process->cpu_state->cs & 3));

    return current_process;
}
