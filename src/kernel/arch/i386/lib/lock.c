#include <ipc.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void lock(volatile lock_t *v)
{
    while (__sync_lock_test_and_set(&v->status, 1))
        yield_to(v->owner);

    v->owner = (current_process == NULL) ? -1 : current_process->pid;
}


void unlock(volatile lock_t *v)
{
    __sync_lock_release(&v->status);
}


bool trylock(volatile lock_t *v)
{
    if (__sync_lock_test_and_set(&v->status, 1))
        return false;
    else
    {
        v->owner = (current_process == NULL) ? -1 : current_process->pid;
        return true;
    }
}
