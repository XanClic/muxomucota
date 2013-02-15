#include <arch-constants.h>
#include <kassert.h>
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
    ptrdiff_t offset;
    uintptr_t phys[];
};


uintptr_t vmmc_create_shm(size_t sz)
{
    int pages = (sz + PAGE_SIZE - 1) / PAGE_SIZE;

    struct shm_sg *sg = kmalloc(sizeof(*sg) + pages * sizeof(uintptr_t));

    sg->size = sz;
    sg->users = 0;
    sg->offset = 0;

    return (uintptr_t)sg;
}


uintptr_t vmmc_make_shm(vmm_context_t *context, int count, void **vaddr_list, int *page_count_list, ptrdiff_t offset)
{
    kassert_print(count > 0, "count: %i", count);

    // Wie soll man damit sonst umgehen?
    kassert_print((offset >= 0) && (offset < PAGE_SIZE), "offset: %p", offset);


    int pages = 0;

    for (int i = 0; i < count; i++)
        pages += page_count_list[i];

    struct shm_sg *sg = kmalloc(sizeof(*sg) + pages * sizeof(uintptr_t));

    sg->size = pages * PAGE_SIZE;
    sg->users = 1;
    sg->offset = offset;

    int k = 0;

    for (int i = 0; i < count; i++)
    {
        for (int j = 0; j < page_count_list[i]; j++, k++)
        {
            uintptr_t dst;
            uintptr_t vaddr = ((uintptr_t)vaddr_list[i] & ~(PAGE_SIZE - 1)) + (uintptr_t)j * PAGE_SIZE;

            int map_flags = vmmc_address_mapped(context, (void *)vaddr, &dst);
            kassert_print((map_flags & (VMM_UR | VMM_UW)) || !map_flags, "vaddr: %p", vaddr);

            if (!map_flags)
            {
                kassert_exec_print(vmmc_do_lazy_map(context, (void *)vaddr), "vaddr: %p", vaddr);
                kassert_print(vmmc_address_mapped(context, (void *)vaddr, &dst) & (VMM_UR | VMM_UW), "vaddr: %p", vaddr);
            }

            // Sanity checks
            kassert_print(vaddr < PHYS_BASE, "vaddr: %p\nPHYS_BASE: %p", vaddr, PHYS_BASE);
            kassert_print(pmm_alloced(dst), "dst: %p", dst);

            sg->phys[k] = dst;
        }
    }

    return (uintptr_t)sg;
}


void *vmmc_open_shm(vmm_context_t *context, uintptr_t shm_id, unsigned flags)
{
    struct shm_sg *sg = (struct shm_sg *)shm_id;

    int pages = (sg->size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (sg->users)
        for (int i = 0; i < pages; i++)
            pmm_use(sg->phys[i]);
    else
        for (int i = 0; i < pages; i++)
            sg->phys[i] = pmm_alloc();

    sg->users++;

    return (void *)((uintptr_t)vmmc_user_map_sg(context, sg->phys, pages, flags) + sg->offset);
}


void vmmc_close_shm(vmm_context_t *context, uintptr_t shm_id, void *virt)
{
    struct shm_sg *sg = (struct shm_sg *)shm_id;

    int pages = (sg->size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int i = 0; i < pages; i++)
        pmm_free(sg->phys[i]);

    vmmc_user_unmap(context, (void *)((uintptr_t)virt - sg->offset), sg->size);

    if (!--sg->users)
        kfree(sg);
}


void vmmc_unmake_shm(uintptr_t shm_id)
{
    struct shm_sg *sg = (struct shm_sg *)shm_id;

    if (!--sg->users)
        kfree(sg);
}


size_t vmmc_get_shm_size(uintptr_t shm_id)
{
    return ((struct shm_sg *)shm_id)->size;
}


void *context_sbrk(vmm_context_t *context, intptr_t inc)
{
    if (!inc)
        return context->heap_end;

    if (inc & ~0xF)
    {
        if (inc >= 0)
            inc = (inc + 0xF) & ~0xF;
        else
            inc = inc & ~0xF;
    }

    uintptr_t old_heap_end = (uintptr_t)context->heap_end;
    uintptr_t new_heap_end = old_heap_end + inc;

    context->heap_end = (void *)new_heap_end;

    uintptr_t ret = old_heap_end;

    if (inc >= 0)
    {
        old_heap_end &= ~(PAGE_SIZE - 1);
        new_heap_end = (new_heap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uintptr_t page = old_heap_end; page < new_heap_end; page += PAGE_SIZE)
        {
            uintptr_t dummy;
            if (!vmmc_address_mapped(context, (void *)page, &dummy))
                vmmc_map_user_page(context, (void *)page, pmm_alloc(), VMM_UR | VMM_UW);
        }
    }
    else
    {
        old_heap_end = (old_heap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        new_heap_end = (new_heap_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uintptr_t page = new_heap_end; page < old_heap_end; page += PAGE_SIZE)
        {
            uintptr_t phys;
            if (vmmc_address_mapped(context, (void *)page, &phys))
            {
                pmm_free(phys);
                vmmc_user_unmap(context, (void *)page, PAGE_SIZE);
            }
        }
    }

    return (void *)ret;
}
