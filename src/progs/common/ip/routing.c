#include <ipc.h>
#include <lock.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <sys/types.h>

#include "routing.h"


struct routing_table_entry *routing_table = NULL;

lock_t routing_table_lock = LOCK_INITIALIZER;


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


    lock(&routing_table_lock);

    struct routing_table_entry *rtep;
    for (rtep = routing_table; rtep != NULL; rtep = rtep->next)
    {
        if ((rtep->dest == rte->dest) && (rtep->mask == rte->mask) && !strcmp(rtep->iface, rte->iface))
        {
            rtep->gw = rte->gw;

            free(rte);

            unlock(&routing_table_lock);

            return 1;
        }
    }

    rte->next = routing_table;
    routing_table = rte;

    unlock(&routing_table_lock);


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


    lock(&routing_table_lock);

    struct routing_table_entry **rtep;
    for (rtep = &routing_table; *rtep != NULL; rtep = &(*rtep)->next)
    {
        if (((*rtep)->dest == rte->dest) && ((*rtep)->mask == rte->mask) && !strcmp((*rtep)->iface, rte->iface))
        {
            struct routing_table_entry *trte = *rtep;
            *rtep = trte->next;
            free(trte);

            free(rte);

            unlock(&routing_table_lock);

            return 1;
        }
    }

    unlock(&routing_table_lock);

    free(rte);


    return 0;
}


static uintmax_t list_routes(void)
{
    pid_t pid = getppid();

    lock(&routing_table_lock);

    for (struct routing_table_entry *rte = routing_table; rte != NULL; rte = rte->next)
        ipc_message_synced(pid, 0, rte, sizeof(*rte) + strlen(rte->iface) + 1);

    unlock(&routing_table_lock);

    return 0;
}


void register_routing_ipc(void)
{
    popup_message_handler(FIRST_NON_VFS_IPC_FUNC + 0, &add_route);
    popup_message_handler(FIRST_NON_VFS_IPC_FUNC + 1, &del_route);
    popup_ping_handler   (FIRST_NON_VFS_IPC_FUNC + 2, &list_routes);
}
