#include <cstdint>
#include <lock.hpp>
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
