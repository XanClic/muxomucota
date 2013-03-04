#include <kassert.h>
#include <cpu.h>
#include <ipc.h>
#include <isr.h>
#include <kmalloc.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>


extern process_t *dead, *daemons, *runqueue;


static void reaper(void)
{
    for (;;)
    {
        // Prozessleichen aufräumen
        if (dead != NULL)
            sweep_dead_processes();

        // IRQs bereitmachen, nachdem IRQ-Threads fertig geworden sind
        for (process_t *p = daemons; p != NULL; p = p->next)
            if ((p->handled_irq >= 0) && (p->settle_count > 0) && (p->status == PROCESS_DAEMON))
                do
                    irq_settled(p->handled_irq);
                while (__sync_sub_and_fetch(&p->settle_count, 1));

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
