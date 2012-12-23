#include <assert.h>
#include <ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
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


int main(int argc, char *argv[])
{
    for (int i = 0; i < (int)ARR_LEN(required_services); i++)
        wait_for(required_services[i]);


    assert(create_pipe("(tty)/tty0", O_RDONLY) == STDIN_FILENO);
    assert(create_pipe("(tty)/tty0", O_WRONLY) == STDOUT_FILENO);
    assert(create_pipe("(tty)/tty0", O_WRONLY) == STDERR_FILENO);


    puts("Hallo Microkernelwelt!");

    puts("Hallo Newline!");


    printf("%i Argument(e):\n", argc);

    for (int i = 0; i < argc; i++)
        printf(" - %s\n", argv[i]);


    pid_t child = fork();

    if (child)
        printf("Vater, Kind hat PID %i.\n", child);
    else
        printf("Kindprozess.\n");


    return 0;
}
