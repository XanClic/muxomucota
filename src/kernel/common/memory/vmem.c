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
