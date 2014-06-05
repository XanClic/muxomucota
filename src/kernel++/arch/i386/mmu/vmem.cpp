#include <cstdint>
#include <cstring>
#include <lock.hpp>
#include <pmm.hpp>
#include <sudoku.hpp>
#include <vmem.hpp>

#include <arch-constants.hpp>
#include <arch-vmem.hpp>


extern uint32_t hpstructs[];

static uint32_t *kpd, *kpt;
static mu::lock kpt_lock;


void arch_init_virtual_memory(vmm_context &primctx)
{
    primctx.initialize(reinterpret_cast<uintptr_t>(hpstructs) - PHYS_BASE, hpstructs);


    kpd = hpstructs;
    kpt = kpd + 1024;


    unsigned i = 0;

    for (; i < (PHYS_BASE >> 22); i++) {
        kpd[i] = MAP_NP;
    }

    for (; i < (HMEM_BASE >> 22); i++) {
        kpd[i] = ((static_cast<uintptr_t>(i) << 22) - PHYS_BASE) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC | MAP_4M;
    }

    for (; i < (UNMP_BASE >> 22); i++) {
        kpd[i] = ((reinterpret_cast<uintptr_t>(kpt) - PHYS_BASE) + (i - (HMEM_BASE >> 22)) * PAGE_SIZE) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC | MAP_4K;
    }

    kpd[i] = MAP_NP;


    for (i = 0; i < ((UNMP_BASE - HMEM_BASE) >> 12); i++) {
        kpt[i] = MAP_NP;
    }


    primctx.switch_to();


    init_gdt();
}


void arch_vmm_context::switch_to(void)
{
    __asm__ __volatile__ ("mov cr3,%0" :: "r"(cr3) : "memory");
}

static void invlpg(uintptr_t address)
{
    __asm__ __volatile__ ("invlpg [%0];" :: "r"(address) : "memory");
}


void *kernel_map(uintptr_t phys, size_t length)
{
    if (phys + length <= HMEM_BASE - PHYS_BASE) {
        return reinterpret_cast<void *>(phys | PHYS_BASE);
    }

    uintptr_t off = phys & PAGE_MASK;
    phys &= ~PAGE_MASK;

    int pages = (length + PAGE_SIZE - 1) >> PAGE_SHIFT;


    // TODO: Optimize
    for (int i = 0; i < static_cast<int>((UNMP_BASE - HMEM_BASE) >> PAGE_SHIFT); i++) {
        if (kpt[i] == MAP_NP) {
            kpt_lock.acquire();

            int j;
            for (j = 0; j < pages; j++) {
                if (kpt[i + j] != MAP_NP) {
                    break;
                }
            }

            if (kpt[i + j] != MAP_NP) {
                kpt_lock.release();
                i += j;
                continue;
            }

            for (j = 0; j < pages; j++) {
                kpt[i + j] = (phys + j * PAGE_SIZE) | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC;
            }

            kpt_lock.release();

            return reinterpret_cast<void *>(HMEM_BASE + static_cast<uintptr_t>(i) * PAGE_SIZE + off);
        }
    }

    SUDOKU("Virtual kernel memory exhausted");

    return nullptr;
}


void *kernel_map(uintptr_t *phys, int frames)
{
    for (int i = 0; i < static_cast<int>((UNMP_BASE - HMEM_BASE) >> PAGE_SHIFT); i++) {
        if (kpt[i] == MAP_NP) {
            if (frames == 1) {
                if (!__sync_bool_compare_and_swap(&kpt[i], MAP_NP, phys[0] | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC)) {
                    continue;
                }
            } else {
                kpt_lock.acquire();

                int j;
                for (j = 0; j < frames; j++) {
                    if (kpt[i + j] != MAP_NP) {
                        break;
                    }
                }

                if (j < frames) {
                    kpt_lock.release();
                    i += j;
                    continue;
                }

                for (j = 0; j < frames; j++) {
                    kpt[i + j] = phys[j] | MAP_PR | MAP_RW | MAP_OS | MAP_GB | MAP_CC;
                }

                kpt_lock.release();
            }

            return reinterpret_cast<void *>(HMEM_BASE + static_cast<uintptr_t>(i) * PAGE_SIZE);
        }
    }

    SUDOKU("Virtual kernel memory exhausted");

    return nullptr;
}


uintptr_t kernel_unmap(void *virt, size_t length)
{
    uintptr_t ivirt = reinterpret_cast<uintptr_t>(virt);

    if (ivirt < HMEM_BASE) {
        return ivirt;
    }


    int bi = (ivirt - HMEM_BASE) >> PAGE_SHIFT;
    int pages = (length + (ivirt & PAGE_MASK) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    uintptr_t phys = (kpt[bi] & ~PAGE_MASK) | (ivirt & PAGE_MASK);


    kpt_lock.acquire();

    for (int i = 0; i < pages; i++) {
        kpt[bi + i] = MAP_NP;
        invlpg((static_cast<uintptr_t>(bi + i) << PAGE_SHIFT) + HMEM_BASE);
    }

    kpt_lock.release();


    return phys;
}


void vmm_context::map_unlocked(void *virt, uintptr_t phys, unsigned flags)
{
    if (IS_KERNEL(virt)) {
        SUDOKU("Tried to map kernel page");
    }

    uintptr_t ivirt = reinterpret_cast<uintptr_t>(virt);

    unsigned pdi =  ivirt >> 22;
    unsigned pti = (ivirt >> 12) & 0x3ff;

    uint32_t *pt;

    if (pd[pdi] & MAP_PR) {
        pt = static_cast<uint32_t *>(kernel_map(pd[pdi] & ~PAGE_MASK, PAGE_SIZE));
    } else {
        unsigned lazy_flags = 0;
        if (pd[pdi] & MAP_US) {
            lazy_flags = (pd[pdi] & MAP_RW) | MAP_US;
        }

        pd[pdi] = physptr::alloc() | MAP_PR | (lazy_flags ? lazy_flags : MAP_RW | MAP_US) | MAP_CC | MAP_LC | MAP_4K;
        pt = static_cast<uint32_t *>(kernel_map(pd[pdi] & ~PAGE_MASK, PAGE_SIZE));
        memsetptr(pt, lazy_flags, PAGE_SIZE / sizeof(uintptr_t));
    }

    pt[pti] = (phys & ~PAGE_MASK) | MAP_PR |
              (flags & (VMM_UW | VMM_PW) ? MAP_RW : 0) |
              (flags & (VMM_UR | VMM_UW | VMM_UX) ? MAP_US : 0) |
              MAP_CC | MAP_LC;

    kernel_unmap(pt, PAGE_SIZE);
}


void *vmm_context::map(uintptr_t *phys, int frames, unsigned flags)
{
    int current_block_size = 0;
    int pdi, pti = 0;

    l.acquire();

    for (pdi = USER_MAP_BASE >> 22; pdi < static_cast<int>(KERNEL_BASE >> 22); pdi++) {
        uint32_t *pt;

        if (pd[pdi] & MAP_PR) {
            pt = static_cast<uint32_t *>(kernel_map(pd[pdi] & ~PAGE_MASK, PAGE_SIZE));

            for (pti = 0; (pti < 1024) && (current_block_size < frames); pti++) {
                if (pt[pti] & MAP_PR) {
                    current_block_size = 0;
                } else {
                    current_block_size++;
                }
            }

            kernel_unmap(pt, PAGE_SIZE);
        } else {
            current_block_size += 1024;
            pti = 1024;
        }

        // Don't want to put this into the loop condition, as this would
        // increment pdi before checking this
        if (current_block_size >= frames) {
            break;
        }
    }

    if (current_block_size < frames) {
        SUDOKU("Virtual kernel memory exhausted");

        l.release();
        return nullptr;
    }

    uintptr_t end = (static_cast<uintptr_t>(pdi) << 22) | (static_cast<uintptr_t>(pti) << 12);
    uintptr_t start = end - (static_cast<uintptr_t>(current_block_size) * PAGE_SIZE);

    // TODO: Awwww this is bad
    for (int i = 0; i < frames; i++) {
        map_unlocked(reinterpret_cast<void *>(start + static_cast<uintptr_t>(i) * PAGE_SIZE), phys[i] & ~PAGE_MASK, flags);
    }

    l.release();

    return reinterpret_cast<void *>(start);
}


unsigned vmm_context::mapped(void *virt, uintptr_t *phys)
{
    uintptr_t ivirt = reinterpret_cast<uintptr_t>(virt);

    unsigned pdi =  ivirt >> 22;
    unsigned pti = (ivirt >> 12) & 0x3ff;

    l.acquire();

    if (!(pd[pdi] & MAP_PR)) {
        l.release();
        return 0;
    }

    uint32_t *pt = static_cast<uint32_t *>(kernel_map(pd[pdi] & ~PAGE_MASK, PAGE_SIZE));

    if (!(pt[pti] & MAP_PR)) {
        kernel_unmap(pt, PAGE_SIZE);
        l.release();
        return 0;
    }

    if (phys) {
        *phys = (pt[pti] & ~PAGE_MASK) | (ivirt & PAGE_MASK);
    }


    unsigned flags = 0;

    switch (pt[pti] & (MAP_RW | MAP_US)) {
        case 0:
            flags = VMM_PR | VMM_PX;
            break;
        case MAP_RW:
            flags = VMM_PR | VMM_PW | VMM_PX;
            break;
        case MAP_US:
            flags = VMM_PR | VMM_PX | VMM_UR | VMM_UX;
            break;
        case MAP_RW | MAP_US:
            flags = VMM_PR | VMM_PW | VMM_PX | VMM_UR | VMM_UW | VMM_UX;
            break;
    }

    l.release();


    kernel_unmap(pt, 4096);

    return flags;
}


void vmm_context::unmap(void *virt, size_t length)
{
    uintptr_t ivirt = reinterpret_cast<uintptr_t>(virt);

    unsigned pdi_start =  ivirt >> 22;
    unsigned pdi_end   = (ivirt + length + (1 << 22) - 1) >> 22;

    l.acquire();

    for (unsigned pdi = pdi_start; pdi < pdi_end; pdi++) {
        if (!(pd[pdi & MAP_PR])) {
            continue;
        }

        unsigned pti_start = (pdi >  pdi_start ) ?     0 :  (ivirt                  >> 12) & 0x3ff;
        unsigned pti_end   = (pdi < pdi_end - 1) ? 0x400 : ((ivirt + length + 4095) >> 12) & 0x3ff;

        uint32_t *pt = static_cast<uint32_t *>(kernel_map(pd[pdi] & ~PAGE_MASK, PAGE_SIZE));

        for (unsigned pti = pti_start; pti < pti_end; pti++) {
            pt[pti] = MAP_NP;
            invlpg((static_cast<uintptr_t>(pdi) << 22) + (static_cast<uintptr_t>(pti) << 12));
        }

        kernel_unmap(pt, PAGE_SIZE);

        if ((pti_start == 0) && (pti_end == 1024)) {
            --physptr(pd[pdi] & ~PAGE_MASK).users;
            pd[pdi] = MAP_NP;
        }
    }

    l.release();
}
