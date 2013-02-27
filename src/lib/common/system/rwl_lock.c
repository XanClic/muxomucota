#include <ipc.h>
#include <lock.h>
#include <unistd.h>


void rwl_lock_r(volatile rw_lock_t *rwl)
{
    lock(&rwl->lock_lock);

    if (!rwl->reader_count++)
        lock(&rwl->write_lock);

    unlock(&rwl->lock_lock);
}


void rwl_unlock_r(volatile rw_lock_t *rwl)
{
    lock(&rwl->lock_lock);

    if (!--rwl->reader_count)
        unlock(&rwl->write_lock);
    else if (rwl->write_lock == getpid())
        rwl->write_lock = -1;

    unlock(&rwl->lock_lock);
}


void rwl_lock_w(volatile rw_lock_t *rwl)
{
    lock(&rwl->write_lock);
}


void rwl_unlock_w(volatile rw_lock_t *rwl)
{
    unlock(&rwl->write_lock);
}


void rwl_require_w(volatile rw_lock_t *rwl)
{
    for (;;)
    {
        while ((rwl->reader_count > 1) || !trylock(&rwl->lock_lock))
            yield_to(rwl->write_lock);

        if (rwl->reader_count == 1)
            break;

        unlock(&rwl->lock_lock);
    }

    rwl->reader_count = 0;

    unlock(&rwl->lock_lock);
}


void rwl_drop_w(volatile rw_lock_t *rwl)
{
    rwl->reader_count = 1;
}
