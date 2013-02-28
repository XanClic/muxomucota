#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <sys/types.h>

#include "arp.h"
#include "interfaces.h"
#include "routing.h"


struct routing_table_entry *routing_table = NULL;

rw_lock_t routing_table_lock = RW_LOCK_INITIALIZER;


bool find_route(uint32_t ip, struct interface **ifc, uint64_t *mac, struct interface *on_ifc)
{
    rwl_lock_r(&routing_table_lock);

    struct routing_table_entry *best_match = NULL;

    for (struct routing_table_entry *rte = routing_table; rte != NULL; rte = rte->next)
    {
        if (((rte->dest & rte->mask) == (ip & rte->mask)) &&
            ((on_ifc == NULL) || (locate_interface(rte->iface) == on_ifc)) &&
            ((best_match == NULL) || (__builtin_popcount(rte->mask) > __builtin_popcount(best_match->mask))))
        {
            best_match = rte;
        }
    }

    rwl_unlock_r(&routing_table_lock);

    if (best_match == NULL)
        return false;

    struct interface *i = locate_interface(best_match->iface);
    if (i == NULL)
        return false;

    *ifc = i;

    if (!~(ip & ~best_match->mask))
        *mac = UINT64_C(0xffffffffffff);
    else
        *mac = arp_lookup(i, best_match->gw ? best_match->gw : ip);

    if (!*mac)
        return false;

    return true;
}


static uintmax_t add_route(void)
{
    struct routing_table_entry *rte;
    size_t size = popup_get_message(NULL, 0);

    if (size < sizeof(*rte) + 1)
        return 0;

    rte = malloc(size);
    popup_get_message(rte, size);

    if (rte->iface[size - sizeof(*rte) - 1])
    {
        free(rte);
        return 0;
    }


    rwl_lock_r(&routing_table_lock);

    struct routing_table_entry *rtep;
    for (rtep = routing_table; rtep != NULL; rtep = rtep->next)
    {
        if ((rtep->dest == rte->dest) && (rtep->mask == rte->mask) && !strcmp(rtep->iface, rte->iface))
        {
            rtep->gw = rte->gw;

            free(rte);

            rwl_unlock_r(&routing_table_lock);

            return 1;
        }
    }

    rwl_require_w(&routing_table_lock);

    rte->next = routing_table;
    routing_table = rte;

    rwl_unlock_w(&routing_table_lock);


    return 1;
}


static uintmax_t del_route(void)
{
    struct routing_table_entry *rte;
    size_t size = popup_get_message(NULL, 0);

    if (size < sizeof(*rte) + 1)
        return 0;

    rte = malloc(size);
    popup_get_message(rte, size);

    if (rte->iface[size - sizeof(*rte) - 1])
    {
        free(rte);
        return 0;
    }


    rwl_lock_r(&routing_table_lock);

    struct routing_table_entry **rtep;
    for (rtep = &routing_table; *rtep != NULL; rtep = &(*rtep)->next)
    {
        if (!strcmp((*rtep)->iface, rte->iface) && ((!rte->dest && !rte->gw && !rte->mask) || (((*rtep)->dest == rte->dest) && ((*rtep)->mask == rte->mask))))
        {
            struct routing_table_entry *trte = *rtep;

            rwl_require_w(&routing_table_lock);

            *rtep = trte->next;

            rwl_drop_w(&routing_table_lock);

            free(trte);

            if (*rtep == NULL)
                break;
        }
    }

    rwl_unlock_r(&routing_table_lock);

    free(rte);


    return 0;
}


static uintmax_t list_routes(void)
{
    pid_t pid = getppid();

    rwl_lock_r(&routing_table_lock);

    for (struct routing_table_entry *rte = routing_table; rte != NULL; rte = rte->next)
        ipc_message_synced(pid, 0, rte, sizeof(*rte) + strlen(rte->iface) + 1);

    rwl_unlock_r(&routing_table_lock);

    return 0;
}


void register_routing_ipc(void)
{
    popup_message_handler(FIRST_NON_VFS_IPC_FUNC + 0, &add_route);
    popup_message_handler(FIRST_NON_VFS_IPC_FUNC + 1, &del_route);
    popup_ping_handler   (FIRST_NON_VFS_IPC_FUNC + 2, &list_routes);
}
