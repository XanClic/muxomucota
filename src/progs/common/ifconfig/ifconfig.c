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
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


static void print_ip(uint32_t ip)
{
    for (int i = 0; i < 4; i++, ip <<= 8)
    {
        printf("%u", ip >> 24);
        if (i < 3)
            putchar('.');
    }
}


static void print_mac(uint64_t mac)
{
    for (int i = 0; i < 6; i++, mac >>= 8)
    {
        printf("%02x", (int)(mac & 0xff));
        if (i < 5)
            putchar(':');
    }
}


static int64_t get_ip(char *s)
{
    uint32_t ip = 0;

    for (int i = 0; i < 4; i++)
    {
        int val = strtol(s, &s, 10);
        if ((val < 0) || (val > 255) || ((*s != '.') && (i < 3)) || (*s && (i == 3)))
            return -1;

        ip = (ip << 8) | val;
        s++;
    }

    return ip;
}


struct routing_table_entry
{
    void *rsvd;

    uint32_t dest, mask, gw;

    char iface[];
};


static void print_if_info(const char *name)
{
    char *ethname, *ipname;

    asprintf(&ethname, "(eth)/%s", name);
    asprintf( &ipname,  "(ip)/%s", name);

    int ethfd = create_pipe(ethname, O_RDONLY), ipfd = create_pipe(ipname, O_RDONLY);

    free(ethname);
    free(ipname);

    if (ethfd < 0)
    {
        if (ipfd >= 0)
            destroy_pipe(ipfd, 0);

        perror(ethname);
        return;
    }

    if (ipfd < 0)
    {
        destroy_pipe(ethfd, 0);

        perror(ipname);
        return;
    }


    if (!pipe_implements(ethfd, I_ETHERNET))
    {
        destroy_pipe(ethfd, 0);
        destroy_pipe( ipfd, 0);

        fprintf(stderr, "%s: Is not an ethernet interface.\n", ethname);
        return;
    }

    if (!pipe_implements(ipfd, I_IP))
    {
        destroy_pipe(ethfd, 0);
        destroy_pipe( ipfd, 0);

        fprintf(stderr, "%s: Is not an IP interface.\n", ipname);
        return;
    }


    uint64_t mac = pipe_get_flag(ethfd, F_MY_MAC);
    uint32_t ip  = pipe_get_flag(ipfd,  F_MY_IP);


    destroy_pipe(ethfd, 0);
    destroy_pipe( ipfd, 0);


    printf("%s: inet ", name);
    print_ip(ip);
    printf("  ether ");
    print_mac(mac);
    printf("\n");
}


int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (argc == 2)
            print_if_info(argv[1]);
        else
        {
            int64_t ip = get_ip(argv[2]);

            if (ip < 0)
            {
                fprintf(stderr, "%s is not a valid IP address.\n", argv[2]);
                return 1;
            }


            char *ipname;
            asprintf(&ipname, "(ip)/%s", argv[1]);

            int ipfd = create_pipe(ipname, O_RDWR);

            free(ipname);

            if (ipfd < 0)
            {
                perror(ipname);
                return 1;
            }

            if (!pipe_implements(ipfd, I_IP))
            {
                fprintf(stderr, "%s: Is not an IP interface.\n", ipname);
                return 1;
            }

            pipe_set_flag(ipfd, F_MY_IP, ip);

            destroy_pipe(ipfd, 0);


            pid_t pid = find_daemon_by_name("ip");

            size_t rte_sz = sizeof(struct routing_table_entry) + strlen(argv[1]) + 1;
            struct routing_table_entry *rte = malloc(rte_sz);
            strcpy(rte->iface, argv[1]);

            rte->dest = 0;
            rte->mask = 0;
            rte->gw   = 0;

            ipc_message_synced(pid, FIRST_NON_VFS_IPC_FUNC + 1, rte, rte_sz);


            rte->dest = ip & 0xffffff00;
            rte->mask = 0xffffff00;
            rte->gw   = 0;

            ipc_message_synced(pid, FIRST_NON_VFS_IPC_FUNC, rte, rte_sz);


            rte->dest = 0;
            rte->mask = 0;
            rte->gw   = (ip & 0xffffff00) | 0x01;

            ipc_message_synced(pid, FIRST_NON_VFS_IPC_FUNC, rte, rte_sz);

            free(rte);
        }
    }
    else
    {
        int fd = create_pipe("(eth)", O_RDONLY);

        if (fd < 0)
        {
            perror("(eth)");
            return 1;
        }

        size_t ifs_sz = pipe_get_flag(fd, F_PRESSURE);
        char ifs[ifs_sz];

        stream_recv(fd, ifs, ifs_sz, O_BLOCKING);

        destroy_pipe(fd, 0);

        for (int i = 0; ifs[i]; i += strlen(&ifs[i]) + 1)
        {
            print_if_info(&ifs[i]);
            putchar('\n');
        }
    }

    return 0;
}
