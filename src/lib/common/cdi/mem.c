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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <cdi.h>
#include <cdi/mem.h>
#include <drivers/memory.h>


struct cdi_mem_area *cdi_mem_alloc(size_t size, cdi_mem_flags_t flags)
{
    struct cdi_mem_area *area = malloc(sizeof(*area));

    area->size  = size;
    area->osdep = flags;


    if (flags & CDI_MEM_VIRT_ONLY)
    {
        area->vaddr = malloc(size);

        area->paddr.num   = 0;
        area->paddr.items = NULL;

        return area;
    }


    area->paddr.num   = 1;
    area->paddr.items = malloc(sizeof(*area->paddr.items));

    uintptr_t max = (uintptr_t)-1;

    if (flags & CDI_MEM_DMA_16M)
        max = (16 << 20) - 1;
    else if (flags & CDI_MEM_DMA_4G)
        max = (UINT64_C(4) << 30) - 1;

    area->paddr.items[0].start = phys_alloc(size, max, flags & CDI_MEM_ALIGN_MASK);

    if (!area->paddr.items[0].start)
    {
        free(area->vaddr);
        free(area);

        return NULL;
    }

    area->paddr.items[0].size = size;


    area->vaddr = map_memory(area->paddr.items[0].start, size, VMM_UR | VMM_UW);


    return area;
}


void cdi_mem_free(struct cdi_mem_area *p)
{
    if (p->osdep & CDI_MEM_VIRT_ONLY)
        free(p->vaddr);
    else
    {
        phys_free(p->paddr.items[0].start, p->size);
        unmap_memory(p->vaddr, p->size);

        free(p->paddr.items);
    }

    free(p);
}


struct cdi_mem_area *cdi_mem_map(uintptr_t paddr, size_t size)
{
    struct cdi_mem_area *area = malloc(sizeof(*area));

    area->size  = size;
    area->osdep = CDI_MEM_PHYS_CONTIGUOUS;

    area->paddr.num   = 1;
    area->paddr.items = malloc(sizeof(*area->paddr.items));

    area->paddr.items[0].start = paddr;
    area->paddr.items[0].size  = size;

    area->vaddr = map_memory(paddr, size, VMM_UR | VMM_UW);


    return area;
}


struct cdi_mem_area *cdi_mem_require_flags(struct cdi_mem_area *p, cdi_mem_flags_t flags)
{
    // lol ihr mich auch
    struct cdi_mem_area *area = cdi_mem_alloc(p->size, flags);

    if (area == NULL)
        return NULL;

    memcpy(area->vaddr, p->vaddr, p->size);

    return area;
}


int cdi_mem_copy(struct cdi_mem_area *dest, struct cdi_mem_area *src)
{
    // lol ihr mich auch
    memmove(dest->vaddr, src->vaddr, dest->size);

    return 0;
}
