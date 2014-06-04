#ifndef _ARCH_VMEM_HPP
#define _ARCH_VMEM_HPP

#include <cstdint>
#include <lock.hpp>


#define MAP_PR (1u << 0)
#define MAP_NP (!MAP_PR)
#define MAP_RW (1u << 1)
#define MAP_RO (!MAP_RW)
#define MAP_US (1u << 2)
#define MAP_OS (!MAP_US)
#define MAP_WT (1u << 3)
#define MAP_NC (1u << 4)
#define MAP_CC (!MAP_NC)
#define MAP_4M (1u << 7)
#define MAP_4K (!MAP_4M)
#define MAP_GB (1u << 8)
#define MAP_LC (!MAP_GB)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SIZE - 1)


#define IS_ALIGNED(addr) ((reinterpret_cast<uintptr_t>(addr) & PAGE_MASK) == 0)


class arch_vmm_context {
    public:
        void initialize(uintptr_t c, uint32_t *p) { cr3 = c; pd = p; }
        void switch_to(void);

    protected:
        mu::lock l;
        uintptr_t cr3;
        uint32_t *pd;
};


void init_gdt(void);

#endif
