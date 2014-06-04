#ifndef _VMEM_HPP
#define _VMEM_HPP

#include <cstddef>
#include <cstdint>
#include <arch-vmem.hpp>


// Unprivilegiert
#define VMM_UR (1 << 0)
#define VMM_UW (1 << 1)
#define VMM_UX (1 << 2)
// Privilegiert
#define VMM_PR (1 << 3)
#define VMM_PW (1 << 4)
#define VMM_PX (1 << 5)


class vmm_context: public arch_vmm_context {
    public:
        void set_users(int users) { uc = users; }

        unsigned mapped(void *virt, uintptr_t *phys);

    private:
        void *he;
        int uc;
};


void init_virtual_memory(void);

void arch_init_virtual_memory(vmm_context &primctx);

void *kernel_map(uintptr_t phys, size_t length);
template<typename T> T *kernel_map(uintptr_t phys)
{ return static_cast<T *>(kernel_map(phys, sizeof(T))); }

uintptr_t kernel_unmap(void *virt, size_t length);
template<typename T> uintptr_t kernel_unmap(T *obj)
{ return kernel_unmap(obj, sizeof(T)); }

#endif
