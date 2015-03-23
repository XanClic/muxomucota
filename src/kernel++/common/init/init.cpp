#include <boot.hpp>
#include <isr.hpp>
#include <platform.hpp>
#include <pmm.hpp>
#include <vmem.hpp>


extern "C" void main(boot_info *bi);

void main(boot_info *bi)
{
    init_virtual_memory();

    init_platform();

    if (!bi->inspect()) {
        return;
    }

    init_pmm();


    init_interrupts();
}
