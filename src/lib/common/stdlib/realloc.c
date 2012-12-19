#include <stddef.h>
#include <stdlib.h>
#include <string.h>


struct free_block
{
    struct free_block *next;
    size_t size;
};


void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);

    if (!size)
    {
        free(ptr);
        return NULL;
    }

    size_t osize = ((struct free_block *)((uintptr_t)ptr - 16))->size;

    if (osize >= size)
        return ptr;

    void *nmem = malloc(size);
    if (nmem == NULL)
        return NULL;

    memcpy(nmem, ptr, osize);
    free(ptr);

    return nmem;
}
