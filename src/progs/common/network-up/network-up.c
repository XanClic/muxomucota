#include <ipc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <sys/wait.h>


int main(void)
{
    printf("Starting network stack...\n");

    const char *init_cmds[] = { "eth", "ip", "udp" };

    for (int i = 0; i < (int)(sizeof(init_cmds) / sizeof(init_cmds[0])); i++)
        if (!fork())
            execlp(init_cmds[i], init_cmds[i], NULL);

    for (int i = 0; i < (int)(sizeof(init_cmds) / sizeof(init_cmds[0])); i++)
        while (find_daemon_by_name(init_cmds[i]) < 0)
            yield();


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

    for (int i = 0; ifcs[i]; i += strlen(&ifcs[i]) + 1)
    {
        printf("%s ", &ifcs[i]);
        fflush(stdout);

        char eth_udp[strlen(&ifcs[i]) + 7], ip[strlen(&ifcs[i]) + 6];

        sprintf(eth_udp, "(eth)/%s", &ifcs[i]);
        sprintf(ip, "(ip)/%s", &ifcs[i]);

        pid_t pid;
        if (!(pid = fork()))
            execlp("mount", "mount", eth_udp, ip, NULL);

        waitpid(pid, NULL, 0);

        sprintf(eth_udp, "(udp)/%s", &ifcs[i]);

        if (!(pid = fork()))
            execlp("mount", "mount", ip, eth_udp, NULL);

        waitpid(pid, NULL, 0);
    }


    putchar('\n');


    return 0;
}
