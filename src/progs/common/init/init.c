#include <errno.h>
#include <ipc.h>
#include <sys/types.h>


int main(void)
{
    pid_t console;

    do
        console = find_daemon_by_name("tty");
    while (console == -1);

    ipc_message(console, 0, "Hallo, Microkernelwelt!", 23);

    return 0;
}
