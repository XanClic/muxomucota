#include <assert.h>
#include <lock.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <syscall.h>


struct free_block
{
    struct free_block *next;
    size_t size;
};

volatile lock_t __heap_lock = LOCK_INITIALIZER;

struct free_block *__first_free = NULL;


void *malloc(size_t sz)
{
    if (!sz)
        sz = 1;

    if (sz & 0xF)
        sz = (sz + 0xF) & ~0xF;

    lock(&__heap_lock);

    struct free_block **fb = &__first_free;

    while (*fb != NULL)
    {
        if ((*fb)->size >= sz)
        {
            struct free_block *t = *fb;

            if (t->size - sz < 32)
                *fb = t->next;
            else
            {
                size_t osize = t->size;
                *fb = (struct free_block *)((uintptr_t)t + sz + 16);
                (*fb)->size = osize - (sz + 16);
                (*fb)->next = t->next;

                t->size = sz;
            }

            unlock(&__heap_lock);

            return (void *)((uintptr_t)t + 16);
        }

        fb = &(*fb)->next;
    }

    unlock(&__heap_lock);

    struct free_block *t = sbrk(sz + 16);
    assert(t);
    t->size = sz;

    return (void *)((uintptr_t)t + 16);
}


// TODO: Neu schreiben. Das ist hässlich wie die Nacht.
void *aligned_alloc(size_t align, size_t size)
{
    // TODO: Überprüfen, ob align eine Zweierpotenz ist

    if (align < 0x10)
        align = 0x10;

    if (size & 0xF)
        size = (size + 0xF) & ~0xF;

    lock(&__heap_lock);

    struct free_block **fb = &__first_free;

    while (*fb != NULL)
    {
        struct free_block *t = *fb;

        size_t align_space = align - ((uintptr_t)t & (align - 1));

        // Sonst kann nicht ordentlich eingerückt werden
        if ((align_space != 16) && (align_space < 48))
            align_space += align;

        if (t->size >= align_space + size)
        {
            size_t osize = t->size;

            struct free_block *rf = NULL;

            if (align_space == 16)
            {
                // Erster Block: Alignter Block
                if (osize - size >= 32)
                {
                    t->size = size;

                    // Zweiter Block: freier Platz am Ende
                    rf = (struct free_block *)((uintptr_t)t + size + 16);
                    rf->size = osize - (size + 16);
                }
            }
            else
            {
                // Erster Block: Alignment
                t->size = align_space - 32;

                // Zweiter Block: Alignter Block
                t = (struct free_block *)((uintptr_t)t + align_space - 16);
                t->size = size;

                // Dritter Block: Evtl. freier Platz am Ende
                if (osize - align_space - size >= 32)
                {
                    rf = (struct free_block *)((uintptr_t)t + 16 + size);
                    rf->size = osize - align_space - size - 16;
                }
            }

            unlock(&__heap_lock);

            if (rf != NULL)
                free((void *)((uintptr_t)rf + 16));

            return (void *)((uintptr_t)t + 16);
        }

        fb = &(*fb)->next;
    }

    // Genug Platz für Alignment
    struct free_block *t = sbrk(size + 48 + align);

    size_t align_space = align - ((uintptr_t)t & (align - 1));

    if ((align_space != 16) && (align_space < 48))
        align_space += align;

    if (align_space == 16)
    {
        t->size = size;

        struct free_block *rf = (struct free_block *)((uintptr_t)t + size + 16);
        rf->size = 32 + align;

        unlock(&__heap_lock);

        free((void *)((uintptr_t)rf + 16));

        return (void *)((uintptr_t)t + 16);
    }
    else
    {
        struct free_block *rf1 = t, *rf2 = NULL;
        rf1->size = align_space - 32;

        t = (struct free_block *)((uintptr_t)t + align_space - 16);
        t->size = size;

        // Freier Platz am Ende
        if (align + 16 >= align_space)
        {
            rf2 = (struct free_block *)((uintptr_t)t + size + 16);
            rf2->size = align + 16 - align_space;
        }
        else
            t->size += align + 48 - align_space;

        unlock(&__heap_lock);

        free((void *)((uintptr_t)rf1 + 16));

        if (rf2 != NULL)
            free((void *)((uintptr_t)rf2 + 16));

        return (void *)((uintptr_t)t + 16);
    }
}
