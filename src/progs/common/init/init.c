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

    void *msg = aligned_alloc(4096, 24);

    strcpy(msg, "Hallo, Microkernelwelt!");

    int pc = 1;

    uintptr_t shm = shm_make(1, &msg, &pc);

    ipc_shm_synced(console, 0, shm);

    shm_close(shm, msg);

    free(msg);

    return 0;
}
