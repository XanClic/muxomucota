/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

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


void *krealloc(void *ptr, size_t size)
{
    if (!ptr) {
        return kmalloc(size);
    }

    size_t *szptr = (size_t *)ptr - 1;
    kassert(!((uintptr_t)szptr & 0xFFF));
    size_t csz = *szptr;

    if (size < csz) {
        return ptr;
    }

    int old_frame_count = (csz  + PAGE_SIZE) >> PAGE_SHIFT;
    int new_frame_count = (size + PAGE_SIZE) >> PAGE_SHIFT;

    if (old_frame_count == new_frame_count) {
        *szptr = size;
        return ptr;
    }

    void *nptr = kmalloc(size);
    memcpy(nptr, ptr, csz);

    return nptr;
}
