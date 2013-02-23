#include <compiler.h>
#include <kassert.h>
#include <lock.h>
#include <process.h>
#include <stddef.h>


extern lock_t runqueue_lock, dead_lock;

extern process_t *runqueue, *dead, *idle_process;


process_t *schedule(pid_t switch_to)
{
    static unsigned schedule_tick = 0;


    if (!trylock(&runqueue_lock))
        return current_process;


    if (unlikely(current_process == NULL))
    {
        if (idle_process != NULL)
            current_process = idle_process;
        else
        {
            unlock(&runqueue_lock);
            return NULL;
        }
    }


    current_process->schedule_tick = schedule_tick++;


    if ((current_process == idle_process) || (current_process->status != PROCESS_ACTIVE))
        current_process = runqueue;


    if (current_process == NULL)
    {
        unlock(&runqueue_lock);
        return idle_process;
    }


    process_t *switch_target = NULL;

    unsigned max_sched_diff = 0;
    process_t *eldest = NULL;


    for (process_t *proc = current_process->rq_next; proc != current_process; proc = proc->rq_next)
    {
        if (proc->status != PROCESS_ACTIVE)
            continue;

        if (proc->pid == switch_to)
            switch_target = proc;

        if (schedule_tick - proc->schedule_tick > max_sched_diff)
        {
            max_sched_diff = schedule_tick - proc->schedule_tick;
            eldest = proc;
        }
    }


    if (eldest == NULL)
    {
        current_process = idle_process;

        unlock(&runqueue_lock);

        return current_process;
    }


    current_process = eldest;

    if ((switch_target != NULL) && (max_sched_diff < 42))
        current_process = switch_target;


    unlock(&runqueue_lock);

    return current_process;
}
