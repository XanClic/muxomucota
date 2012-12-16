#ifndef _VMEM_H
#define _VMEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include <arch-vmem.h>


// Unprivilegiert
#define VMM_UR (1 << 0)
#define VMM_UW (1 << 1)
#define VMM_UX (1 << 2)
// Privilegiert
#define VMM_PR (1 << 3)
#define VMM_PW (1 << 4)
#define VMM_PX (1 << 5)


typedef struct
{
    struct arch_vmm_context_info arch;

    void *heap_end;

    int users;
} vmm_context_t;


extern vmm_context_t kernel_context;


void init_virtual_memory(void);

void arch_init_virtual_memory(vmm_context_t *primordial_context);

void change_vmm_context(const vmm_context_t *context);
void create_vmm_context(vmm_context_t *context);
void create_vmm_context_arch(vmm_context_t *context);
void destroy_vmm_context(vmm_context_t *context);

void use_vmm_context(vmm_context_t *context);
void unuse_vmm_context(vmm_context_t *context);

void vmmc_set_heap_top(vmm_context_t *context, void *address);

void *kernel_map(uintptr_t phys, size_t length);
// non-consecutive mapping
void *kernel_map_nc(uintptr_t *pageframe_list, int frames);
uintptr_t kernel_unmap(void *virt, size_t length);

void vmmc_map_user_page_unlocked(const vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags);
void vmmc_map_user_page(vmm_context_t *context, void *virt, uintptr_t phys, unsigned flags);
void vmmc_lazy_map_area(vmm_context_t *context, void *virt, ptrdiff_t sz, unsigned flags);

void *vmmc_user_map(vmm_context_t *context, uintptr_t phys, size_t length, unsigned flags);

bool vmmc_do_cow(vmm_context_t *context, void *address);
bool vmmc_do_lazy_map(vmm_context_t *context, void *address);

#endif
