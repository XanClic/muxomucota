#include <boot.h>
#include <isr.h>
#include <platform.h>
#include <pmm.h>
#include <prime-procs.h>
#include <process.h>
#include <stdbool.h>
#include <string.h>
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


    char *cmdline = strdup(get_kernel_command_line());

    char *prime = NULL;

    for (char *tok = strtok(cmdline, " "); tok != NULL; tok = strtok(NULL, " "))
        if (!strncmp(tok, "prime=", 6))
            prime = tok + 6;


    int prime_proc_load_count = (prime == NULL) ? 0 : strtok_count(prime, ",");
    char *prime_proc_names[prime_proc_load_count];

    if (prime != NULL)
        strtok_array(prime_proc_names, prime, ",");


    int prime_procs = prime_process_count();

    for (int i = 0; i < prime_procs; i++)
    {
        void *img_start;
        size_t img_size;
        char name_arr[256];

        fetch_prime_process(i, &img_start, &img_size, name_arr, sizeof(name_arr));


        int argc = strtok_count(name_arr, " ");
        const char *argv[argc];

        strtok_array((char **)argv, name_arr, " ");


        if (prime == NULL)
            create_process_from_image(argc, argv, img_start);
        else
        {
            bool found = false;

            for (int j = 0; j < prime_proc_load_count; j++)
                if (!strcmp(argv[0], prime_proc_names[j]))
                    found = true;


            if (found)
                create_process_from_image(argc, argv, img_start);
        }
    }


    enter_idle_process();
}
