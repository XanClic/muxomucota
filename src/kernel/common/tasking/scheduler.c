#include <compiler.h>
#include <lock.h>
#include <process.h>
#include <stddef.h>


extern lock_t runqueue_lock;

extern process_t *runqueue, *idle_process;


process_t *schedule(void)
{
    if (runqueue_lock == locked)
        return current_process;

#if 0
    if (current_process == NULL)
        *((volatile short *)0xF00B8000) = '/' | 0xF00;
    else
    {
        *((volatile short *)0xF00B8000) = (current_process->pid + 48) | 0xF00;

        volatile short *v = (volatile short *)0xF00B80A0;
        int i;
        for (i = 0; (i < 80) && (i < sizeof(current_process->name)) && current_process->name[i]; i++)
            v[i] = current_process->name[i] | 0xF00;
        for (; i < 80; i++)
            v[i] = ' ' | 0xF00;
    }

    __asm__ __volatile__ ("" ::: "memory");
#endif


    if (unlikely(current_process == NULL))
    {
        if (idle_process != NULL)
            current_process = idle_process;
        else
            return NULL;
    }


    if (current_process == idle_process)
        current_process = runqueue;


    if (current_process == NULL)
        return idle_process;


    process_t *initial = current_process;

    do
    {
        if (current_process->runqueue_next->status == PROCESS_DESTRUCT)
        {
            process_t *the_process_after_next = current_process->runqueue_next->runqueue_next;

            if (runqueue == current_process->runqueue_next)
                runqueue = current_process;

            destroy_process_struct(current_process->runqueue_next);

            if (current_process->runqueue_next == current_process)
            {
                runqueue = NULL;
                current_process = idle_process;
                break;
            }
            else
                current_process->runqueue_next = the_process_after_next;
        }

        current_process = current_process->runqueue_next;

        if ((current_process->status != PROCESS_ACTIVE) && (current_process == initial))
            current_process = idle_process;
    }
    while (current_process->status != PROCESS_ACTIVE);


    return current_process;
}
