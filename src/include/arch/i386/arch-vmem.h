#ifndef _ARCH_VMEM_H
#define _ARCH_VMEM_H

#include <lock.h>
#include <stdint.h>


#define MAP_PR (1U << 0)
#define MAP_NP (!MAP_PR)
#define MAP_RW (1U << 1)
#define MAP_RO (!MAP_RW)
#define MAP_US (1U << 2)
#define MAP_OS (!MAP_US)
#define MAP_WT (1U << 3)
#define MAP_NC (1U << 4)
#define MAP_CC (!MAP_NC)
#define MAP_4M (1U << 7)
#define MAP_4K (!MAP_4M)
#define MAP_GB (1U << 8)
#define MAP_LC (!MAP_GB)

#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)


#define IS_ALIGNED(addr) (((uintptr_t)(addr) & (PAGE_SIZE - 1)) == 0)


struct arch_vmm_context_info
{
    uintptr_t cr3;
    uint32_t *pd;

    lock_t lock;
};


void init_gdt(void);

#endif
