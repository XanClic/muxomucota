#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


struct free_block
{
    struct free_block *next;
    size_t size;
};


size_t _alloced_size(void *ptr)
{
    return ((struct free_block *)((uintptr_t)ptr - 16))->size;
}
