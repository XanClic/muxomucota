#include <cpu.h>
#include <ipc.h>
#include <kmalloc.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>


extern process_t *dead, *runqueue;


static void reaper(void)
{
    for (;;)
    {
        if (dead != NULL)
            sweep_dead_processes();

        yield();
    }
}


void enter_idle_process(void)
{
    run_process(create_kernel_thread("reaper", reaper));


    make_idle_process();

    for (;;)
    {
        bool runqueue_empty = true;

        for (process_t *proc = runqueue; proc != NULL; proc = proc->next)
        {
            if (proc->status == PROCESS_ACTIVE)
            {
                runqueue_empty = false;
                break;
            }
        }

        if (runqueue_empty)
            cpu_halt();
        else
            yield();
    }
}
