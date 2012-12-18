#include <kmalloc.h>
#include <pmm.h>
#include <vmem.h>

#include <arch-vmem.h>


vmm_context_t kernel_context = { .users = 1 };

void init_virtual_memory(void)
{
    arch_init_virtual_memory(&kernel_context);
}


void create_vmm_context(vmm_context_t *context)
{
    context->users = 0;
    context->heap_end = NULL;

    create_vmm_context_arch(context);
}


void use_vmm_context(vmm_context_t *context)
{
    context->users++;
}


void unuse_vmm_context(vmm_context_t *context)
{
    if (!--context->users)
        destroy_vmm_context(context);
}


void vmmc_set_heap_top(vmm_context_t *context, void *address)
{
    context->heap_end = address;
}


struct shm_sg
{
    size_t size;
    int users;
    uintptr_t phys[];
};


uintptr_t vmmc_create_shm(size_t sz)
{
    int pages = (sz + PAGE_SIZE - 1) / PAGE_SIZE;

    struct shm_sg *sg = kmalloc(sizeof(*sg) + pages * sizeof(uintptr_t));

    sg->size = sz;
    sg->users = 0;

    return (uintptr_t)sg;
}


void *vmmc_open_shm(vmm_context_t *context, uintptr_t shm_id, unsigned flags)
{
    struct shm_sg *sg = (struct shm_sg *)shm_id;

    int pages = (sg->size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (sg->users)
        for (int i = 0; i < pages; i++)
            pmm_use(sg->phys[i], 1);
    else
        for (int i = 0; i < pages; i++)
            sg->phys[i] = pmm_alloc(1);

    sg->users++;

    // FIXME: Ohne VMM_UW könnte ein Schreibzugriff zu COW führen
    return vmmc_user_map_sg(context, sg->phys, pages, flags);
}


void vmmc_close_shm(vmm_context_t *context, uintptr_t shm_id, void *virt)
{
    struct shm_sg *sg = (struct shm_sg *)shm_id;

    int pages = (sg->size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int i = 0; i < pages; i++)
        pmm_free(sg->phys[i], 1);

    vmmc_user_unmap(context, virt, sg->size);

    if (!--sg->users)
        kfree(sg);
}


size_t vmmc_get_shm_size(uintptr_t shm_id)
{
    return ((struct shm_sg *)shm_id)->size;
}
