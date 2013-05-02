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

#include <lock.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>

#include "interfaces.h"


static struct interface *interfaces = NULL;

static rw_lock_t interface_lock = RW_LOCK_INITIALIZER;


struct interface *locate_interface(const char *name)
{
    rwl_lock_r(&interface_lock);

    for (struct interface *i = interfaces; i != NULL; i = i->next)
    {
        if (!strcmp(i->name, name))
        {
            rwl_unlock_r(&interface_lock);
            return i;
        }
    }

    rwl_unlock_r(&interface_lock);


    char fname[7 + strlen(name)];
    sprintf(fname, "(eth)/%s", name);

    int fd = create_pipe(fname, O_RDWR);

    if (fd < 0)
        return NULL;

    pipe_set_flag(fd, F_IPC_SIGNAL, true);
    pipe_set_flag(fd, F_ETH_PACKET_TYPE, 0x0800);


    struct interface *i = malloc(sizeof(*i));

    lock_init(&i->lock);

    i->name = strdup(name);
    i->fd = fd;
    i->ip = 0;

    rwl_lock_w(&interface_lock);

    i->next = interfaces;
    interfaces = i;

    rwl_unlock_w(&interface_lock);


    return i;
}


struct interface *find_interface(int fd)
{
    rwl_lock_r(&interface_lock);

    for (struct interface *i = interfaces; i != NULL; i = i->next)
    {
        if (i->fd == fd)
        {
            rwl_unlock_r(&interface_lock);
            return i;
        }
    }

    rwl_unlock_r(&interface_lock);

    return NULL;
}
