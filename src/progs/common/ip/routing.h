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
