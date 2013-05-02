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

#ifndef ROUTING_H
#define ROUTING_H

#include <lock.h>
#include <stdint.h>

#include "interfaces.h"


struct routing_table_entry
{
    struct routing_table_entry *next;

    uint32_t dest, mask;
    uint32_t gw;

    char iface[];
};


extern struct routing_table_entry *routing_table;
extern rw_lock_t routing_table_lock;


void register_routing_ipc(void);

bool find_route(uint32_t ip, struct interface **ifc, uint64_t *mac, struct interface *on_ifc);

#endif
