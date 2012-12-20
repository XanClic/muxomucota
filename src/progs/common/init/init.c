#include <ipc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vfs.h>


// TODO: Per argv übergeben.
static const char *required_services[] = {
    "tty"
};


static void wait_for(const char *name)
{
    while (find_daemon_by_name(name) < 0)
        yield();
}


int main(void)
{
    for (int i = 0; i < (int)ARR_LEN(required_services); i++)
        wait_for(required_services[i]);


    char msg[] = "Hallo Microkernelwelt!";


    int fd = create_pipe("(tty)/tty0", 0);

    stream_send(fd, msg, sizeof(msg), 0);

    destroy_pipe(fd, 0);

    return 0;
}
