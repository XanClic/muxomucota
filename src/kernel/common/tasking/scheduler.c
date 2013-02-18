#include <compiler.h>
#include <lock.h>
#include <process.h>
#include <stddef.h>


extern lock_t runqueue_lock, dead_lock;

extern process_t *runqueue, *dead, *idle_process;


process_t *schedule(pid_t switch_to)
{
    if (IS_LOCKED(runqueue_lock))
        return current_process;


    if (unlikely(current_process == NULL))
    {
        if (idle_process != NULL)
            current_process = idle_process;
        else
            return NULL;
    }


    if ((current_process == idle_process) || (current_process->status != PROCESS_ACTIVE))
        current_process = runqueue;


    if (current_process == NULL)
        return idle_process;


    if (switch_to > 0)
    {
        process_t *target = find_process(switch_to);

        if ((target != NULL) && (target->status == PROCESS_ACTIVE))
            return (current_process = target);
    }


    process_t *initial = current_process;

    do
    {
        current_process = current_process->rq_next;

        if ((current_process->status != PROCESS_ACTIVE) && (current_process == initial))
            current_process = idle_process;
    }
    while (current_process->status != PROCESS_ACTIVE);


    return current_process;
}
