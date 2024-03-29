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
#include <memmap.h>
#include <pmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>

#include <arch-constants.h>


#define BITMAP_COW_FLAG (1 << 7)
#define BITMAP_USERS    (0xFF & ~BITMAP_COW_FLAG)


// #define RANDOMIZE_AT_BOOT


static uintptr_t mem_base = (uintptr_t)-1;
static int mem_entries;
uint8_t *bitmap;

static int look_from_here;


void init_pmm(void)
{
    uintptr_t memsize = 0;

    int mmap_entries = memmap_length();

    for (int i = 0; i < mmap_entries; i++)
    {
        bool usable;
        uintptr_t base;
        size_t length;


        get_memmap_info(i, &usable, &base, &length);

        // Vorzeichenloser Überlauf ist dankenswerterweise definiert
        if (base + length < base)
            length = 0xFFFFFFFFU - base;

        if (usable && (base + length > memsize))
            memsize = base + length;

        if (usable && (base < mem_base))
            mem_base = base;
    }


    mem_entries = (memsize - mem_base) >> PAGE_SHIFT;

    uintptr_t kernel_paddr, kernel_end;
    get_kernel_dim(&kernel_paddr, &kernel_end);

    kernel_paddr -= PHYS_BASE;
    kernel_paddr = kernel_paddr >> PAGE_SHIFT;

    kernel_end -= PHYS_BASE;
    kernel_end = (kernel_end + PAGE_SIZE - 1) >> PAGE_SHIFT;


    // Liegt der Kernel am Beginn des benutzbaren Bereichs, so kann die Bitmap auch entsprechend verschoben werden
    if (((kernel_paddr << PAGE_SHIFT) <= mem_base) && ((kernel_end << PAGE_SHIFT) > mem_base))
        mem_base = kernel_end << PAGE_SHIFT;


    // Auf volle Pageframes aufrunden
    mem_base = (mem_base + PAGE_SIZE - 1) & ~((1 << PAGE_SHIFT) - 1);


    int bmp_size = (mem_entries * sizeof(*bitmap) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    bitmap = kernel_map(kernel_end << PAGE_SHIFT, (size_t)bmp_size << PAGE_SHIFT);
    kernel_end += bmp_size;

    // Alles als belegt markieren
    memset(bitmap, 0xff, mem_entries * sizeof(*bitmap));


    // Nur benutzbare Einträge der Memory Map als belegt markieren
    for (int i = 0; i < mmap_entries; i++)
    {
        bool usable;
        uintptr_t base;
        size_t length;

        get_memmap_info(i, &usable, &base, &length);
        if (usable && (base < memsize) && (base + length > mem_base))
        {
            int top;

            if (base + length + PAGE_SIZE - 1 < base) // Überlauf
                top = mem_entries;
            else
                top = (base + length + PAGE_SIZE - 1 - mem_base) >> PAGE_SHIFT;

            if (top > mem_entries)
                top = mem_entries;

            int paddr = (base > mem_base) ? ((base - mem_base) >> PAGE_SHIFT) : 0;

            if (paddr > mem_entries) // Whyever
                continue;

            for (int bi = paddr; bi < top; bi++)
                bitmap[bi] = 0;
        }
    }


    // Wenn der Kernel im von der Bitmap abgedeckten Bereich liegt
    if ((kernel_end > (mem_base >> PAGE_SHIFT)) && (kernel_paddr < (unsigned)(mem_entries + (mem_base >> PAGE_SHIFT))))
    {
        int s = kernel_paddr - (mem_base >> PAGE_SHIFT);
        int e = kernel_end - (mem_base >> PAGE_SHIFT);

        if (e > mem_entries)
            e = mem_entries;
        if (s < 0)
            s = 0;

        //Speicher für den Kernel reservieren
        for (int bi = s; bi < e; bi++)
            bitmap[bi] = BITMAP_USERS;
    }


    look_from_here = 0;


#ifdef RANDOMIZE_AT_BOOT
    for (uintptr_t paddr = mem_base; paddr < memsize; paddr += PAGE_SIZE)
    {
        if (!bitmap[(paddr - mem_base) >> PAGE_SHIFT])
        {
            void *va = kernel_map(paddr, PAGE_SIZE);
            memset(va, 42, PAGE_SIZE); // Chosen by reading a good book
            kernel_unmap(va, PAGE_SIZE);
        }
    }
#endif
}


uintptr_t pmm_alloc(void)
{
    bool first_occurance = true;


    for (int i = look_from_here; i < mem_entries; i++)
    {
        if (!bitmap[i])
        {
            if (__sync_fetch_and_add(&bitmap[i], 1))
                bitmap[i]--;
            else
            {
                if (first_occurance)
                    look_from_here = i;

                return mem_base + ((uintptr_t)i << PAGE_SHIFT);
            }

            first_occurance = false;
        }
    }


    kassert_print(0, "physical memory exhausted", 0);


    return 0;
}


void pmm_free(uintptr_t paddr)
{
    kassert_print(!(paddr & (PAGE_SIZE - 1)), "paddr=%p", paddr);


    int base = (paddr - mem_base) >> PAGE_SHIFT;

    kassert_print(bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\nbitmap[base]=%p", paddr, base, (uintptr_t)bitmap[base]);
    if (!--bitmap[base] && (look_from_here > base))
        look_from_here = base;
}


void pmm_use(uintptr_t paddr)
{
    kassert_print(!(paddr & (PAGE_SIZE - 1)), "paddr=%p", paddr);


    int base = (paddr - mem_base) >> PAGE_SHIFT;

    kassert_print(bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\nbitmap[base]=%p", paddr, base, (uintptr_t)bitmap[base]); // Wenn das bisher unused ist, ist das doof.
    kassert_print((bitmap[base] & BITMAP_USERS) < BITMAP_USERS, "paddr=%p\nbase=%i\nbitmap[base]=%p", paddr, base, (uintptr_t)bitmap[base]); // Sonst wird das Inkrementieren lustig.

    bitmap[base]++;
}


int pmm_user_count(uintptr_t paddr)
{
    kassert_print(!(paddr & (PAGE_SIZE - 1)), "paddr=%p", paddr);

    return bitmap[(paddr - mem_base) >> PAGE_SHIFT] & BITMAP_USERS;
}


bool pmm_alloced(uintptr_t paddr)
{
    return pmm_user_count(paddr) && (pmm_user_count(paddr) < BITMAP_USERS);
}


void pmm_mark_cow(uintptr_t paddr, bool flag)
{
    kassert_print(!(paddr & (PAGE_SIZE - 1)), "paddr=%p", paddr);


    int base = (paddr - mem_base) >> PAGE_SHIFT;

    kassert_print(bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\nbitmap[base]=%p", paddr, base, (uintptr_t)bitmap[base]);


    if (flag)
        bitmap[base] |=  BITMAP_COW_FLAG;
    else
        bitmap[base] &= ~BITMAP_COW_FLAG;
}


bool pmm_is_cow(uintptr_t paddr)
{
    kassert_print(!(paddr & (PAGE_SIZE - 1)), "paddr=%p", paddr);

    int index = (paddr - mem_base) >> PAGE_SHIFT;

    kassert_print(bitmap[index] & BITMAP_USERS, "paddr=%p\nindex=%i\nbitmap[index]=%p", paddr, index, (uintptr_t)bitmap[index]);

    return !!(bitmap[index] & BITMAP_COW_FLAG);
}


uintptr_t pmm_alloc_advanced(size_t length, uintptr_t max_addr, int alignment)
{
    if (!length)
        return 0;

    int frames = (length + PAGE_SIZE - 1) >> PAGE_SHIFT;

    int end = (max_addr >> PAGE_SHIFT) - frames;


    if (mem_entries < end)
        end = mem_entries;


    uintptr_t real_alignment = (1U << alignment) - 1;


    for (int i = look_from_here; i < end; i++)
    {
        uintptr_t base = mem_base + ((uintptr_t)i << PAGE_SHIFT);

        // TODO: Optimieren
        if (base & real_alignment)
            continue;

        if (!bitmap[i])
        {
            bool is_free = true;

            for (int j = 1; j < frames; j++)
            {
                if (bitmap[i + j])
                {
                    is_free = false;
                    break;
                }
            }

            if (!is_free)
                continue;

            for (int j = 0; j < frames; j++)
            {
                if (__sync_fetch_and_add(&bitmap[i + j], 1))
                {
                    for (int k = 0; k <= j; k++)
                        bitmap[i + k]--;

                    is_free = false;

                    break;
                }
            }

            if (!is_free)
                continue;

            return base;
        }
    }


    return 0;
}
