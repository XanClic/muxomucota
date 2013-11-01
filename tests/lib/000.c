#include <assert.h>
#include <stdio.h>

/* TEST
   { headers: ['stdlib.h', 'lock.h'],
     files: ['malloc', 'free'].map { |f| "stdlib/#{f}.c" } }
 */

void lock(volatile lock_t *v)
{
    assert(!__sync_val_compare_and_swap(v, 0, 1));
}

void unlock(volatile lock_t *v)
{
    __sync_lock_release(v);
}

void *sbrk(intptr_t sz)
{
    return NULL;
}

void abort(void)
{
    fprintf(stderr, "[abort] Exiting gracefully\n");
    exit(0);
}

int main(void)
{
    void *foo = malloc(8192);
    // will never get here
    printf("%p\n", foo);
    free(foo);
    return 0;
}
