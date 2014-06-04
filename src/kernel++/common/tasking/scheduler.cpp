#include <cstddef>
#include <compiler.hpp>
#include <kassert.hpp>
#include <lock.hpp>
#include <process.hpp>


mu::lock schedule_lock;

extern process *runqueue, *idle_process;


process *schedule(pid_t switch_to)
{
    static unsigned schedule_tick = 0;

    process *switch_target = nullptr, *irq_process = nullptr;

    unsigned max_sched_diff = 0;
    process *eldest = nullptr;


    if (!schedule_lock.acquire_nonblock()) {
        return current_process;
    }

    if (unlikely(!current_process)) {
        if (idle_process) {
            current_process = idle_process;
        } else {
            goto return_current;
        }
    }

    current_process->schedule_tick = schedule_tick++;

    if ((current_process == idle_process) || (current_process->status != process::ACTIVE)) {
        current_process = runqueue;
    }

    if (!current_process) {
        current_process = idle_process;
        goto return_current;
    }


    for (process *proc = runqueue; proc; proc = proc->rq_next) {
        if (proc->status != process::ACTIVE) {
            continue;
        }

        if (proc->pid == switch_to) {
            switch_target = proc;
        }

        if ((proc->handled_irq >= 0) && proc->handling_irq && proc->fresh_irq) {
            irq_process = proc;
        }

        if (schedule_tick - proc->schedule_tick > max_sched_diff) {
            max_sched_diff = schedule_tick - proc->schedule_tick;
            eldest = proc;
        }
    }


    if (!eldest) {
        current_process = idle_process;
        goto return_current;
    }

    current_process = eldest;

    if (switch_target && (max_sched_diff < 42)) {
        current_process = switch_target;
    }

    if (irq_process) {
        irq_process->fresh_irq = false;
        current_process = irq_process;
    }


return_current:
    schedule_lock.release();
    return current_process;
}
