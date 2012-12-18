#include <kassert.h>
#include <lock.h>
#include <memmap.h>
#include <pmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vmem.h>

#include <arch-constants.h>


static uintptr_t mem_base = (uintptr_t)-1;
static int mem_entries;
uint8_t *bitmap;

static int look_from_here;

static lock_t bitmap_lock = unlocked;


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

    uintptr_t kernel_start, kernel_end;
    get_kernel_dim(&kernel_start, &kernel_end);

    kernel_start -= PHYS_BASE;
    kernel_start = kernel_start >> PAGE_SHIFT;

    kernel_end -= PHYS_BASE;
    kernel_end = (kernel_end + PAGE_SIZE - 1) >> PAGE_SHIFT;


    // Liegt der Kernel am Beginn des benutzbaren Bereichs, so kann die Bitmap auch entsprechend verschoben werden
    if (((kernel_start << PAGE_SHIFT) <= mem_base) && ((kernel_end << PAGE_SHIFT) > mem_base))
        mem_base = kernel_end << PAGE_SHIFT;


    // Auf volle Pageframes aufrunden
    mem_base = (mem_base + PAGE_SIZE - 1) & ~((1 << PAGE_SHIFT) - 1);


    int bmp_size = (mem_entries * sizeof(*bitmap) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    bitmap = kernel_map(kernel_end << PAGE_SHIFT, bmp_size);
    kernel_end += bmp_size;

    memset(bitmap, 0, mem_entries * sizeof(*bitmap));


    // Unbenutzbare Einträge der Memory Map als belegt markieren
    for (int i = 0; i < mmap_entries; i++)
    {
        bool usable;
        uintptr_t base;
        size_t length;

        get_memmap_info(i, &usable, &base, &length);
        if (!usable)
        {
            int top;

            if (base + length + PAGE_SIZE - 1 < base) // Überlauf
                top = mem_entries;
            else
                top = (base + length + PAGE_SIZE - 1 - mem_base) >> PAGE_SHIFT;

            int start = (base - mem_base) >> PAGE_SHIFT;
            if (top > mem_entries)
                top = mem_entries;

            if (start > mem_entries) // Whyever
                continue;

            if (start < 0)
                start = 0;

            if (top < 0)
                continue;

            for (int bi = start; bi < top; bi++)
                bitmap[bi] = (1 << sizeof(*bitmap)) - 1;
        }
    }


    // Wenn der Kernel im von der Bitmap abgedeckten Bereich liegt
    if ((kernel_end > (mem_base >> PAGE_SHIFT)) && (kernel_start < (unsigned)(mem_entries + (mem_base >> PAGE_SHIFT))))
    {
        int s = kernel_start - (mem_base >> PAGE_SHIFT);
        int e = kernel_end - (mem_base >> PAGE_SHIFT);

        if (e > mem_entries)
            e = mem_entries;
        if (s < 0)
            s = 0;

        //Speicher für den Kernel reservieren
        for (int bi = s; bi < e; bi++)
            bitmap[bi] = (1 << sizeof(*bitmap)) - 1;
    }


    look_from_here = 0;
}


uintptr_t pmm_alloc(int count)
{
    bool first_occurance = true;


    kassert(count > 0);


    for (int i = look_from_here; i < mem_entries; i++)
    {
        if (!bitmap[i])
        {
            kassert_exec(lock(&bitmap_lock));

            int j;
            for (j = 0; j < count; j++)
                if (bitmap[i + j])
                    break;

            if (bitmap[i + j])
            {
                unlock(&bitmap_lock);

                i += j;
                first_occurance = false;

                continue;
            }


            for (j = 0; j < count; j++)
                bitmap[i + j] = 1;

            if (first_occurance)
                look_from_here = i + j;

            unlock(&bitmap_lock);


            return mem_base + ((uintptr_t)i << PAGE_SHIFT);
        }
    }


    kassert(0);


    return 0;
}


void pmm_free(uintptr_t start, int count)
{
    kassert(!(start & (PAGE_SIZE - 1)));
    kassert(count > 0);


    kassert_exec(lock(&bitmap_lock));

    int base = (start - mem_base) >> PAGE_SHIFT;

    for (int i = 0; i < count; i++)
    {
        kassert(bitmap[base + i]);
        if (!--bitmap[base + i] && (look_from_here > base + i))
            look_from_here = base + i;
    }

    unlock(&bitmap_lock);
}


void pmm_use(uintptr_t start, int count)
{
    kassert(!(start & (PAGE_SIZE - 1)));
    kassert(count > 0);


    kassert_exec(lock(&bitmap_lock));

    int base = (start - mem_base) >> PAGE_SHIFT;

    for (int i = 0; i < count; i++)
    {
        kassert(bitmap[base + i]); // Wenn das bisher unused ist, ist das doof.
        bitmap[base + i]++;
    }

    unlock(&bitmap_lock);
}
