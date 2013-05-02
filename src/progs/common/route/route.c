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

#include <drivers.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct routing_table_entry
{
    void *rsvd;

    uint32_t dest, mask, gw;

    char iface[];
};


static void print_ip(uint32_t ip)
{
    char ips[16];

    char *ipp = ips;

    for (int i = 0; i < 4; i++, ip <<= 8)
    {
        sprintf(ipp, "%u", ip >> 24);
        ipp += strlen(ipp);

        if (i < 3)
            *(ipp++) = '.';
    }

    printf("%-16s", ips);
}


static uintmax_t print_route(void)
{
    struct routing_table_entry *rte;
    size_t size = popup_get_message(NULL, 0);

    if (size < sizeof(*rte) + 1)
        return 0;

    rte = malloc(size);

    popup_get_message(rte, size);

    print_ip(rte->dest);
    putchar(' ');
    print_ip(rte->gw);
    putchar(' ');
    print_ip(rte->mask);
    putchar(' ');
    puts(rte->iface);

    free(rte);

    return 0;
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


int main(int argc, char *argv[])
{
    pid_t pid = find_daemon_by_name("ip");

    if (pid < 0)
    {
        fprintf(stderr, "Could not connect to the IP service.\n");
        return 1;
    }


    if (argc == 1)
    {
        popup_message_handler(0, &print_route);

        printf("%-16s %-16s %-16s Iface\n", "Destination", "Gateway", "Netmask");

        ipc_ping_synced(pid, FIRST_NON_VFS_IPC_FUNC + 2);

        return 0;
    }


    if (argc < 4)
    {
        fprintf(stderr, "Usage: route add/del <destination> [gw <gateway>] [netmask <netmask>] [dev] <interface>\n");
        return 1;
    }


    int fnc;

    if (!strcmp(argv[1], "add"))
        fnc = FIRST_NON_VFS_IPC_FUNC + 0;
    else if (!strcmp(argv[1], "del"))
        fnc = FIRST_NON_VFS_IPC_FUNC + 1;
    else
    {
        fprintf(stderr, "Unknown command %s.\n", argv[1]);
        return 1;
    }


    char *ifc = argv[argc - 1];

    struct routing_table_entry *rte = malloc(sizeof(*rte) + strlen(ifc) + 1);

    strcpy(rte->iface, ifc);


    if (!strcmp(argv[2], "default"))
        rte->dest = 0;
    else
    {
        int64_t dest = get_ip(argv[2]);

        if (dest < 0)
        {
            fprintf(stderr, "%s is not a valid IP address.\n", argv[2]);
            return 1;
        }

        rte->dest = dest;
    }


    rte->gw = 0;
    rte->mask = 0;


    for (int i = 3; i < argc - 1; i++)
    {
        if (!strcmp(argv[i], "gw"))
        {
            if (i == argc - 2)
            {
                fprintf(stderr, "Interface name expected.\n");
                return 1;
            }

            int64_t gw = get_ip(argv[++i]);

            if (gw < 0)
            {
                fprintf(stderr, "%s is not a valid IP address.\n", argv[i]);
                return 1;
            }

            rte->gw = gw;
        }
        else if (!strcmp(argv[i], "netmask"))
        {
            if (i == argc - 2)
            {
                fprintf(stderr, "Interface name expected.\n");
                return 1;
            }

            int64_t mask = get_ip(argv[++i]);

            if (mask < 0)
            {
                fprintf(stderr, "%s is not a valid IP address.\n", argv[i]);
                return 1;
            }

            rte->mask = mask;
        }
        else if (!strcmp(argv[i], "dev"))
        {
            if (i < argc - 2)
            {
                fprintf(stderr, "The interface name must be the final parameter.\n");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "Unknown parameter %s.\n", argv[i]);
            return 1;
        }
    }


    if (!ipc_message_synced(pid, fnc, rte, sizeof(*rte) + strlen(rte->iface) + 1))
    {
        fprintf(stderr, "Could not carry command out.\n");
        return 1;
    }

    return 0;
}
