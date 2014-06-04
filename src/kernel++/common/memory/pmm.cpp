#include <cstdint>
#include <cstring>
#include <kassert.hpp>
#include <memmap.hpp>
#include <pmm.hpp>
#include <vmem.hpp>

#include <arch-constants.hpp>


#define BITMAP_COW_FLAG (1 << 7)
#define BITMAP_USERS    (0xff & ~BITMAP_COW_FLAG)


// #define RANDOMIZE_AT_BOOT


static uintptr_t mem_base = static_cast<uintptr_t>(-1);
static int bitmap_entries;
static int look_from_here;
uint8_t *pmm_bitmap;


void init_pmm(void)
{
    uintptr_t memsize = 0;

    for (int i = 0; i < memmap.size; i++) {
        memory_map::entry e = memmap[i];

        if (!e.usable) {
            continue;
        }

        // Thank god unsigned overflow is well-defined
        if (e.start + e.length < e.start) {
            e.length = SIZE_MAX - e.start;
        }

        if (e.start + e.length > memsize) {
            memsize = e.start + e.length;
        }

        if (e.start < mem_base) {
            mem_base = e.start;
        }
    }


    bitmap_entries = (memsize - mem_base) >> PAGE_SHIFT;

    int kernel_start = (memmap.kernel_start() - PHYS_BASE) >> PAGE_SHIFT;
    int kernel_end = (memmap.kernel_end() - PHYS_BASE + PAGE_SIZE - 1) >> PAGE_SHIFT;


    // If the kernel is at the start of the bitmap, we can simply relocate the
    // beginning of the bitmap
    if (((static_cast<uintptr_t>(kernel_start) << PAGE_SHIFT) <= mem_base) && ((static_cast<uintptr_t>(kernel_end) << PAGE_SHIFT) > mem_base)) {
        mem_base = kernel_end << PAGE_SHIFT;
    }


    // Round up to full page frames
    mem_base = (mem_base + PAGE_SIZE - 1) & ~PAGE_MASK;


    int bmp_size = (bitmap_entries * sizeof(*pmm_bitmap) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    pmm_bitmap = static_cast<uint8_t *>(kernel_map(static_cast<uintptr_t>(kernel_end) << PAGE_SHIFT, static_cast<size_t>(bmp_size) << PAGE_SHIFT));
    kernel_end += bmp_size;

    // Mark everything as used
    memset(pmm_bitmap, BITMAP_USERS, bitmap_entries * sizeof(*pmm_bitmap));


    // Then mark usable entries from the memory map as free
    for (int i = 0; i < memmap.size; i++) {
        memory_map::entry e = memmap[i];

        if (e.usable && (e.start < memsize) && (e.start + e.length > mem_base)) {
            int top;

            if (e.start + e.length + PAGE_SIZE - 1 < e.start) {
                // Overflow
                top = bitmap_entries;
            } else {
                top = (e.start + e.length + PAGE_SIZE - 1) >> PAGE_SHIFT;
                if (top > bitmap_entries) {
                    top = bitmap_entries;
                }
            }

            int paddr = (e.start > mem_base) ? ((e.start - mem_base) >> PAGE_SHIFT) : 0;

            if (paddr > bitmap_entries) {
                // I don't even know why this is here
                continue;
            }

            memset(&pmm_bitmap[paddr], 0, top - paddr);
        }
    }


    // Mark kernel as used, if necessary
    if ((kernel_end > static_cast<int>(mem_base >> PAGE_SHIFT)) && (kernel_start < static_cast<int>(bitmap_entries + (mem_base >> PAGE_SHIFT)))) {
        int s = kernel_start - (mem_base >> PAGE_SHIFT);
        int e = kernel_end   - (mem_base >> PAGE_SHIFT);

        if (e > bitmap_entries) {
            e = bitmap_entries;
        }

        if (s < 0) {
            s = 0;
        }

        memset(&pmm_bitmap[s], BITMAP_USERS, e - s);
    }


    look_from_here = 0;


#ifdef RANDOMIZE_AT_BOOT
    for (uintptr_t paddr = mem_base; paddr < memsize; paddr += PAGE_SIZE) {
        if (!bitmap[(paddr - mem_base) >> PAGE_SHIFT]) {
            void *va = kernel_map(paddr, PAGE_SIZE);
            memset(va, 42, PAGE_SIZE); // Chosen by reading a good book
            kernel_unmap(va, PAGE_SIZE);
        }
    }
#endif
}


physptr physptr::alloc(int frame_count, uintptr_t max_addr, int alignment)
{
    if (frame_count <= 0) {
        return nullptr;
    }

    int end = (max_addr >> PAGE_SHIFT) - frame_count;

    if (end > bitmap_entries) {
        end = bitmap_entries;
    }

    uintptr_t real_alignment = (static_cast<uintptr_t>(1) << alignment) - 1;


    bool first_occurance = true;

    for (int i = look_from_here; i < end; i++) {
        uintptr_t base = mem_base + (static_cast<uintptr_t>(i) << PAGE_SHIFT);

        // TODO: Optimize
        if (base & real_alignment) {
            first_occurance = false;
            continue;
        }

        if (!pmm_bitmap[i]) {
            bool is_free = true;

            for (int j = 1; j < frame_count; j++) {
                if (pmm_bitmap[i + j]) {
                    is_free = false;
                    break;
                }
            }

            if (!is_free) {
                first_occurance = false;
                continue;
            }

            for (int j = 0; j < frame_count; j++) {
                if (__sync_fetch_and_add(&pmm_bitmap[i + j], 1)) {
                    for (int k = 0; k <= j; k++) {
                        pmm_bitmap[i + k]--;
                    }

                    is_free = false;
                    break;
                }
            }

            if (!is_free) {
                first_occurance = false;
                continue;
            }

            if (first_occurance) {
                look_from_here = i + frame_count - 1;
            }

            return base;
        }
    }


    kassert_print(0, "Physical memory exhausted", 0);


    return nullptr;
}


void physptr::free(void)
{
    kassert_print(!(address & PAGE_MASK), "paddr=%p", address);


    int base = (address - mem_base) >> PAGE_SHIFT;

    kassert_print(pmm_bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\npmm_bitmap[base]=%p", address, base, static_cast<uintptr_t>(pmm_bitmap[base]));
    if (!--pmm_bitmap[base] && (look_from_here > base)) {
        look_from_here = base;
    }

    if ((pmm_bitmap[base] & BITMAP_COW_FLAG) && ((pmm_bitmap[base] & BITMAP_USERS) <= 1)) {
        pmm_bitmap[base] &= ~BITMAP_COW_FLAG;
    }
}


int physptr::inc_users(void)
{
    if (!address) {
        // I have no idea where I might use this once
        address = physptr::alloc();
        return 1;
    }


    kassert_print(!(address & PAGE_MASK), "paddr=%p", address);


    int base = (address - mem_base) >> PAGE_SHIFT;

    kassert_print(pmm_bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\npmm_bitmap[base]=%p", address, base, static_cast<uintptr_t>(pmm_bitmap[base]));
    kassert_print((pmm_bitmap[base] & BITMAP_USERS) < BITMAP_USERS, "paddr=%p\nbase=%i\npmm_bitmap[base]=%p", address, base, static_cast<uintptr_t>(pmm_bitmap[base]));

    return ++pmm_bitmap[base];
}


int physptr::dec_users(void)
{
    free();
    return users;
}


int physptr::get_users(void)
{
    kassert_print(!(address & PAGE_MASK), "paddr=%p", address);

    return pmm_bitmap[(address - mem_base) >> PAGE_SHIFT] & BITMAP_USERS;
}


bool physptr::alloced(void)
{
    int uc = users;
    return uc && (uc < BITMAP_USERS);
}


void physptr::set_cow(bool flag)
{
    kassert_print(!(address & PAGE_MASK), "paddr=%p", address);


    int base = (address - mem_base) >> PAGE_SHIFT;

    kassert_print(pmm_bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\npmm_bitmap[base]=%p", address, base, static_cast<uintptr_t>(pmm_bitmap[base]));

    if (flag) {
        pmm_bitmap[base] |=  BITMAP_COW_FLAG;
    } else {
        pmm_bitmap[base] &= ~BITMAP_COW_FLAG;
    }
}


bool physptr::get_cow(void)
{
    kassert_print(!(address & PAGE_MASK), "paddr=%p", address);

    int base = (address - mem_base) >> PAGE_SHIFT;

    kassert_print(pmm_bitmap[base] & BITMAP_USERS, "paddr=%p\nbase=%i\npmm_bitmap[base]=%p", address, base, static_cast<uintptr_t>(pmm_bitmap[base]));

    return pmm_bitmap[base] & BITMAP_COW_FLAG;
}
