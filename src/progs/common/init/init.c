#include <errno.h>
#include <ipc.h>
#include <shm.h>
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

    return 0;
}
