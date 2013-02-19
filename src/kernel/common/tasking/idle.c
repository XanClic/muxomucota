#include <cpu.h>
#include <ipc.h>
#include <kmalloc.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>


extern process_t *dead, *runqueue;

extern lock_t runqueue_lock;


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
    run_process(create_kernel_thread("reaper", reaper, kmalloc(1024), 1024));


    make_idle_process();

    for (;;)
    {
#ifdef COOPERATIVE
        bool runqueue_empty = true;

        lock(&runqueue_lock);

        if (runqueue != NULL)
        {
            process_t *proc = runqueue;
            do
            {
                if (proc->status == PROCESS_ACTIVE)
                {
                    runqueue_empty = false;
                    break;
                }
                proc = proc->next;
            }
            while (proc != runqueue);
        }

        unlock(&runqueue_lock);

        if (runqueue_empty)
            cpu_halt();
        else
            yield();
#else
        cpu_halt();
#endif
    }
}
