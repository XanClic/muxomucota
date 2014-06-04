#include <compiler.hpp>
#include <lock.hpp>
#include <process.hpp>
#include <vmem.hpp>


extern process *idle_process;

static mu::lock dispatch_lock;


cpu_state *dispatch(cpu_state *state, pid_t switch_to)
{
    if (unlikely(!dispatch_lock.acquire_nonblock())) {
        return state;
    }

    if (likely(current_process)) {
        current_process->cpu = state;
    } else if (likely(idle_process)) {
        idle_process->cpu = state;
    }

    process *next_process = schedule(switch_to);

    if (unlikely(!next_process)) {
        dispatch_lock.release();
        return state;
    }

    next_process->vmm->switch_to();
    next_process->arch_switch_to();

    dispatch_lock.release();

    return next_process->cpu;
}
