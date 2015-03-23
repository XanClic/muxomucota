#ifndef _VMEM_HPP
#define _VMEM_HPP

#include <cstddef>
#include <cstdint>
#include <voodoo>

#include <arch-vmem.hpp>


// Unprivilegiert
#define VMM_UR (1 << 0)
#define VMM_UW (1 << 1)
#define VMM_UX (1 << 2)
// Privilegiert
#define VMM_PR (1 << 3)
#define VMM_PW (1 << 4)
#define VMM_PX (1 << 5)


#ifdef KERNEL
class vmm_context: public arch_vmm_context {
    public:
        int users;
        void *heap_end;

        vmm_context(void);
        unsigned mapped(void *virt, uintptr_t *phys);

        void map_unlocked(void *virt, uintptr_t phys, unsigned flags);
        void map_area_lazy(void *virt, size_t size, unsigned flags);

        void map(void *virt, uintptr_t phys, unsigned flags);
        void *map(uintptr_t phys, size_t size, unsigned flags);
        void *map(uintptr_t *phys, int pages, unsigned flags);

        void unmap(void *virt, size_t length);

        bool do_cow(void *address);
        bool do_lazy_map(void *address);

        bool is_valid_user_mem(const void *start, size_t length, unsigned flags);

        void *sbrk(ptrdiff_t inc);
};


class vmm_context_reference {
    public:
        vmm_context_reference(void);
        vmm_context_reference(vmm_context *c);
        ~vmm_context_reference(void);

        operator vmm_context *(void) { return context; }
        vmm_context *operator->(void) { return context; }

        void operator=(vmm_context *c);

    private:
        vmm_context *context;
};


void init_virtual_memory(void);

void arch_init_virtual_memory(vmm_context &primctx);

void *kernel_map(uintptr_t phys, size_t length);
template<typename T> T *kernel_map(uintptr_t phys)
{ return static_cast<T *>(kernel_map(phys, sizeof(T))); }

void *kernel_map(uintptr_t *phys, int frames);

uintptr_t kernel_unmap(void *virt, size_t length);
template<typename T> uintptr_t kernel_unmap(T *obj)
{ return kernel_unmap(obj, sizeof(T)); }
#endif

#endif
