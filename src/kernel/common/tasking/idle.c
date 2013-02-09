#include <cpu.h>
#include <lock.h>
#include <process.h>
#include <stddef.h>


extern process_t *dead;

extern lock_t dead_lock;


void enter_idle_process(void)
{
    make_idle_process();

    for (;;)
    {
        if (dead == NULL)
            cpu_halt();

        sweep_dead_processes();
    }
}
