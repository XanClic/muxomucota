#include <ipc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <sys/wait.h>


int main(void)
{
    printf("Starting network stack...\n");

    const char *init_cmds[] = { "eth", "ip", "udp", "tcp", "dns" };

    for (int i = 0; i < (int)(sizeof(init_cmds) / sizeof(init_cmds[0])); i++)
    {
        if (!fork())
            execlp(init_cmds[i], init_cmds[i], NULL);

        while (find_daemon_by_name(init_cmds[i]) < 0)
            yield();
    }


    printf("Starting drivers...\n");

    const char *drivers[] = { "rtl8139", "rtl8168b", "ne2k", "e1000", "sis900", "pcnet" };

    pid_t drv_pid[sizeof(drivers) / sizeof(drivers[0])];

    for (int i = 0; i < (int)(sizeof(drivers) / sizeof(drivers[0])); i++)
        if (!(drv_pid[i] = fork()))
            execlp(drivers[i], drivers[i], NULL);

    for (int i = 0; i < (int)(sizeof(drivers) / sizeof(drivers[0])); i++)
        while ((find_daemon_by_name(drivers[i]) < 0) && (waitpid(drv_pid[i], NULL, WNOHANG) <= 0))
            yield();


    printf("Establishing network stack...\n");

    int fd = create_pipe("(eth)", O_RDONLY);

    size_t ifcs_sz = pipe_get_flag(fd, F_PRESSURE);
    char ifcs[ifcs_sz];

    stream_recv(fd, ifcs, ifcs_sz, O_BLOCKING);

    destroy_pipe(fd, 0);


    for (int i = 0; ifcs[i]; i += strlen(&ifcs[i]) + 1)
    {
        printf("%s ", &ifcs[i]);
        fflush(stdout);

        char fname[strlen(&ifcs[i]) + 7];

        sprintf(fname, "(ip)/%s", &ifcs[i]);
        destroy_pipe(create_pipe(fname, O_CREAT), 0);

        sprintf(fname, "(udp)/%s", &ifcs[i]);
        destroy_pipe(create_pipe(fname, O_CREAT), 0);
    }


    putchar('\n');


    return 0;
}
