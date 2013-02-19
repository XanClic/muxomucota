#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>


void lock(volatile lock_t *v)
{
    pid_t mypid = getpid(), owner;

    while ((owner = __sync_val_compare_and_swap(v, 0, mypid)) > 0)
        yield_to(owner);
}


void unlock(volatile lock_t *v)
{
    __sync_lock_release(v);
}


bool trylock(volatile lock_t *v)
{
    return __sync_bool_compare_and_swap(v, 0, getpid());
}
