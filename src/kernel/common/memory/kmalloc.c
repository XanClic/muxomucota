#include <kassert.h>
#include <kmalloc.h>
#include <pmm.h>
#include <stdint.h>
#include <vmem.h>


void *kmalloc(size_t size)
{
    size += sizeof(size);


    int frame_count = (size + PAGE_SIZE) >> PAGE_SHIFT;
    uintptr_t pageframes[frame_count];

    // Externe Fragmentation vermeiden
    for (int i = 0; i < frame_count; i++)
        pageframes[i] = pmm_alloc();


    size_t *mem = kernel_map_nc(pageframes, sizeof(pageframes) / sizeof(pageframes[0]));

    *mem = size;


    return mem + 1;
}


void kfree(void *ptr)
{
    if (ptr == NULL)
        return;


    ptr = (size_t *)ptr - 1;

    kassert(!((uintptr_t)ptr & 0xFFF));

    size_t size = *(size_t *)ptr;


    int frame_count = (size + PAGE_SIZE) >> PAGE_SHIFT;

    for (int i = 0; i < frame_count; i++)
        pmm_free(kernel_unmap((void *)((uintptr_t)ptr + ((uintptr_t)i << PAGE_SHIFT)), PAGE_SIZE));
}
