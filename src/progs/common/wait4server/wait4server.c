#include <ipc.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Server name required as argument.\n");
        return 1;
    }


    while (find_daemon_by_name(argv[1]) < 0)
        yield();

    return 0;
}
