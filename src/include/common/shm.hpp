#ifndef _SHM_HPP
#define _SHM_HPP

#include <cstddef>
#include <cstdint>
#include <kernel_object.hpp>
#include <vmem.hpp>


#ifndef KERNEL
namespace mu
{
#endif

#ifdef KERNEL
class shm_area {
    public:
        size_t size;
        ptrdiff_t offset;
        int users;

        uintptr_t phys[];


        shm_area(size_t sz);
        shm_area(int count, void **vaddr_list, int *page_count_list, ptrdiff_t offset);
        ~shm_area(void);
};


struct shm_mapping {
    shm_mapping(void);
    ~shm_mapping(void);

    void invalidate(void);

    vmm_context *context;
    void *address;
};
#endif


class shm_reference: public kernel_object {
    public:
#ifdef KERNEL
        shm_area *shm;
        shm_mapping mapping;
#endif


        shm_reference(size_t size);
#ifdef KERNEL
        shm_reference(shm_area *shm);
#endif
        ~shm_reference(void);

#ifdef KERNEL
        void *map(vmm_context *context, unsigned flags = VMM_UR | VMM_UW);
#else
        void *map(unsigned flags = VMM_UR | VMM_UW);
#endif

        void unmap(void);
};

#ifndef KERNEL
}
#endif

#endif
