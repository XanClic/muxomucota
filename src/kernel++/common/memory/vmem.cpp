#include <vmem.hpp>

#include <arch-vmem.hpp>


vmm_context kernel_context;

void init_virtual_memory(void)
{
    kernel_context.set_users(1);

    arch_init_virtual_memory(kernel_context);
}
