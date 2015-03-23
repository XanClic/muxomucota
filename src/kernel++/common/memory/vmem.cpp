#include <cstddef>
#include <cstdint>
#include <kassert.hpp>
#include <kmalloc.hpp>
#include <parith.hpp>
#include <shm.hpp>
#include <vmem.hpp>

#include <arch-constants.hpp>
#include <arch-vmem.hpp>


vmm_context kernel_context;


void init_virtual_memory(void)
{
    kernel_context.users = 1;

    arch_init_virtual_memory(kernel_context);
}


vmm_context::vmm_context(void):
    users(0),
    heap_end(nullptr)
{
}


bool vmm_context::is_valid_user_mem(const void *start, size_t length, unsigned flags)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(start) & ~PAGE_MASK;
    int pages = (length + (reinterpret_cast<uintptr_t>(start) & PAGE_MASK) + PAGE_SIZE - 1) / PAGE_SIZE;

    while (pages--) {
        if (IS_KERNEL(addr)) {
            return false;
        }

        unsigned flags_set = mapped(reinterpret_cast<void *>(addr), nullptr);
        if ((flags_set & flags) != flags) {
            // FIXME: This assumes that RO directly indicates COW
            if (!(flags & (VMM_UW | VMM_PW)) || (flags_set & (VMM_UW | VMM_PW))) {
                return false;
            }
        }

        addr += PAGE_SIZE;
    }

    return true;
}


vmm_context_reference::vmm_context_reference(void):
    context(nullptr)
{
}


vmm_context_reference::vmm_context_reference(vmm_context *c):
    context(c)
{
    context->users++;
}


vmm_context_reference::~vmm_context_reference(void)
{
    if (context && !--context->users) {
        delete context;
    }
}


void vmm_context_reference::operator=(vmm_context *c)
{
    if (context && !--context->users) {
        delete context;
    }

    (context = c)->users++;
}


shm_area::shm_area(size_t sz):
    size(sz),
    offset(0),
    users(0)
{
}


shm_area::~shm_area(void)
{
    kassert_print(!users, "users=%i", users);
}


shm_mapping::shm_mapping(void)
{
    invalidate();
}


shm_mapping::~shm_mapping(void)
{
    kassert_print(!context && !address, "context=%p\naddress=%p", context, address);
}


void shm_mapping::invalidate(void)
{
    context = nullptr;
    address = nullptr;
}


shm_reference::shm_reference(size_t sz):
    kernel_object(sizeof(*this))
{
    int pages = (sz + PAGE_SIZE - 1) / PAGE_SIZE;

    shm = new (kmalloc(sizeof(shm_area) + pages * sizeof(shm_area::phys[0]))) shm_area(sz);
    shm->users++;
}


shm_reference::shm_reference(shm_area *m):
    kernel_object(sizeof(*this)),
    shm(m)
{
    shm->users++;
}


shm_reference::~shm_reference(void)
{
    unmap();
}


void *shm_reference::map(vmm_context *context, unsigned flags)
{
    if (mapping.address) {
        kassert_print(mapping.context == context, "mapping.context=%p\ncontext=%p", mapping.context, context);
        return mapping.address;
    }

    int pages = (shm->size + PAGE_SIZE - 1) / PAGE_SIZE;

    mapping.context = context;
    mapping.address = mapping.context->map(shm->phys, pages, flags);

    return mapping.address;
}


void shm_reference::unmap(void)
{
    if (!mapping.address) {
        return;
    }

    mapping.context->unmap(mapping.address, shm->size);
    mapping.invalidate();
}
