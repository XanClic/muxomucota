/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

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
    }


    putchar('\n');


    return 0;
}
