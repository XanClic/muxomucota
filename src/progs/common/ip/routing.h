#ifndef ROUTING_H
#define ROUTING_H

#include <lock.h>
#include <stdint.h>


struct routing_table_entry
{
    struct routing_table_entry *next;

    uint32_t dest, mask;
    uint32_t gw;

    char iface[];
};


extern struct routing_table_entry *routing_table;
extern lock_t routing_table_lock;


void register_routing_ipc(void);

#endif
