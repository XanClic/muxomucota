#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>

/* TEST
   { headers: ['stdlib.h', 'lock.h'],
     files: ['malloc', 'free'].map { |f| "stdlib/#{f}.c" } }
 */

uintptr_t big_mem_area, brk_ptr;

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
    uintptr_t ret = brk_ptr;
    brk_ptr += sz;
    return (void *)ret;
}

void abort(void)
{
    fprintf(stderr, "[abort] Exiting gracefully\n");
    exit(0);
}

int main(void)
{
    big_mem_area = (uintptr_t)mmap(NULL, 1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (big_mem_area == (uintptr_t)MAP_FAILED)
        return 1;

    brk_ptr = big_mem_area;

    // just test basic functionality

    for (int i = 0; i < 16; i++)
    {
        void *foo = malloc(1048576);
        if ((uintptr_t)foo != big_mem_area + 0x10)
            return 1;
        free(foo);
    }

    // randomized

    for (int i = 0; i < 16; i++)
    {
        void *foo = malloc(rand() % 131072 + 1);
        if ((uintptr_t)foo != big_mem_area + 0x10)
            return 1;
        free(foo);
    }

    // test memory join

    void *f1 = malloc(0x713);
    void *f2 = malloc(0xc58);
    void *f3 = malloc(0x357);

    if ((uintptr_t)f1 != big_mem_area + 0x0010) return 1;
    if ((uintptr_t)f2 != big_mem_area + 0x0740) return 1;
    if ((uintptr_t)f3 != big_mem_area + 0x13b0) return 1;

    free(f1);
    f1 = malloc(0x720);
    if ((uintptr_t)f1 != big_mem_area + 0x0010) return 1;
    free(f1);
    f1 = malloc(0x721);
    if ((uintptr_t)f1 != big_mem_area + 0x1720) return 1;
    free(f1);
    free(f2);
    f1 = malloc(0x721);
    if ((uintptr_t)f1 != big_mem_area + 0x0010) return 1;
    free(f1);
    f1 = malloc(0x1390);
    if ((uintptr_t)f1 != big_mem_area + 0x0010) return 1;
    free(f1);
    f1 = malloc(0x1391);
    if ((uintptr_t)f1 != big_mem_area + 0x1720) return 1;
    free(f1);
    free(f3);

    // test allocation density randomized

    struct
    {
        size_t sz;
        void *ptr;
    } blocks[32];

    srand(time(NULL));
    uintptr_t exp = big_mem_area + 0x10;
    for (int i = 0; i < 32; i++)
    {
        blocks[i].sz = rand() % 16383 + 1;
        blocks[i].ptr = malloc(blocks[i].sz);
        if ((uintptr_t)blocks[i].ptr != exp)
        {
            printf("Block %i: exp %p, got %p\n", i, (void *)exp, blocks[i].ptr);
            return 1;
        }
        exp = (exp + 0x1f + blocks[i].sz) & ~0xf;
    }

    for (int i = 0; i < 32; i++)
    {
        free(blocks[i].ptr);
        blocks[i].ptr = NULL;
    }

    // test overlaps randomized

    int rem = 32;


    for (int i = 0; i < 32; i++)
    {
        blocks[i].sz = rand() % 16383 + 1;
        blocks[i].ptr = malloc(blocks[i].sz);
        memset(blocks[i].ptr, i, blocks[i].sz);
        for (int j = 0; j <= i; j++)
        {
            if (!blocks[j].ptr)
                continue;

            for (int b = 0; b < blocks[j].sz; b++)
                if (((char *)blocks[j].ptr)[b] != j)
                    return 1;
        }

        int r = rand() % 32;
        if (blocks[r].ptr)
        {
            free(blocks[r].ptr);
            blocks[r].ptr = NULL;
            rem--;
        }
    }

    for (; rem > 0; rem--)
    {
        int i = rand() % rem, j;
        for (j = 0; j < 32; j++)
            if (blocks[j].ptr && !i--)
                break;
        free(blocks[j].ptr);
        blocks[j].ptr = NULL;
    }

    // test whether memory is uniform again
    if ((uintptr_t)malloc(1048576) != big_mem_area + 0x10) return 1;

    return 0;
}
