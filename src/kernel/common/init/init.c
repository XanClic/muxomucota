#include <boot.h>
#include <cpu.h>
#include <isr.h>
#include <platform.h>
#include <pmm.h>
#include <prime-procs.h>
#include <process.h>
#include <system-timer.h>
#include <vmem.h>


void main(void *boot_info)
{
    init_virtual_memory();

    init_platform();

    // Sollte auch Informationen zur Memory Map abrufen und die primordialen
    // Prozessimages laden (bspw. Multibootmodule)
    if (!get_boot_info(boot_info))
        return;

    init_pmm();


    init_interrupts();

    init_system_timer();


    int prime_procs = prime_process_count();

    for (int i = 0; i < prime_procs; i++)
    {
        void *img_start;
        size_t img_size;
        char name_arr[32];

        fetch_prime_process(i, &img_start, &img_size, name_arr, sizeof(name_arr));

        // TODO: Kommandozeile parsen

        create_process_from_image(name_arr, img_start, img_size);
    }


    make_idle_process();


    for (;;)
        cpu_halt();
}
