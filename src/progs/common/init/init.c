#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


int main(void)
{
    pid_t console;

    do
        console = find_daemon_by_name("tty");
    while (console == -1);

    uintptr_t shm = shm_create(24);
    char *msg = shm_open(shm, VMM_UW);

    strcpy(msg, "Hallo, Microkernelwelt!");

    ipc_shm_synced(console, 0, shm);

    shm_close(shm, msg);


    void *alloced_garbage = malloc(87);

    void *alloced_mem = aligned_alloc(4096, 4096);

    __asm__ __volatile__ ("" :: "r"(alloced_garbage), "r"(alloced_mem) : "memory");

    for (;;);

    return 0;
}
