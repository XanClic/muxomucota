#include <lock.h>
#include <stdbool.h>
#include <stdint.h>


bool lock(volatile lock_t *v)
{
    while (__sync_lock_test_and_set(v, locked) == locked);

    return true;
}


void unlock(volatile lock_t *v)
{
    __sync_lock_release(v);
}