#include <compiler.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <vfs.h>
#include <arpa/inet.h>

#include "arp.h"
#include "interfaces.h"


#define ARP_CACHE_SIZE 32


struct arp_packet
{
    uint16_t hwaddrtype, protoaddrtype;
    uint8_t hwaddrlen, protoaddrlen;
    uint16_t operation;
    uint8_t senderhwaddr[6];
    uint32_t senderprotoaddr;
    uint8_t targethwaddr[6];
    uint32_t targetprotoaddr;
} cc_packed;


struct cache_entry
{
    struct interface *i;
    uint32_t ip;
    uint64_t mac;

    int last_usage;
};

static struct cache_entry arp_cache[ARP_CACHE_SIZE];

static rw_lock_t arp_cache_lock = RW_LOCK_INITIALIZER;


static uint64_t arp_resolve(struct interface *ifc, uint32_t ip);


uint64_t arp_lookup(struct interface *ifc, uint32_t ip)
{
    static int usage_counter = 0;


    rwl_lock_r(&arp_cache_lock);

    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if ((arp_cache[i].i == ifc) && (arp_cache[i].ip == ip))
        {
            uint64_t mac = arp_cache[i].mac;
            arp_cache[i].last_usage = __sync_fetch_and_add(&usage_counter, 1);

            rwl_unlock_r(&arp_cache_lock);

            return mac;
        }
    }

    rwl_unlock_r(&arp_cache_lock);


    uint64_t mac = arp_resolve(ifc, ip);


    rwl_lock_r(&arp_cache_lock);

    int oldest = usage_counter, oldest_i = 0;

    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (arp_cache[i].last_usage < oldest)
        {
            oldest = arp_cache[i].last_usage;
            oldest_i = i;
        }

        if (arp_cache[i].i == NULL)
        {
            oldest_i = i;
            break;
        }
    }

    rwl_require_w(&arp_cache_lock);

    arp_cache[oldest_i].i   = ifc;
    arp_cache[oldest_i].ip  = ip;
    arp_cache[oldest_i].mac = mac;

    arp_cache[oldest_i].last_usage = __sync_fetch_and_add(&usage_counter, 1);

    rwl_unlock_w(&arp_cache_lock);


    return mac;
}


static volatile uint32_t reply_ip;
static volatile uint64_t reply_mac;


static uint64_t arp_resolve(struct interface *ifc, uint32_t ip)
{
    static lock_t arp_lock = LOCK_INITIALIZER;

    lock(&arp_lock);

    uint64_t srcmac = pipe_get_flag(ifc->fd, F_MY_MAC);

    struct arp_packet pkt = {
        .hwaddrtype = htons(0x0001),
        .protoaddrtype = htons(0x0800),
        .hwaddrlen = 6,
        .protoaddrlen = 4,
        .operation = htons(1),
        .senderprotoaddr = htonl(ifc->ip),
        .targetprotoaddr = htonl(ip)
    };

    for (int i = 0; i < 6; i++)
        pkt.senderhwaddr[i] = (srcmac >> (i * 8)) & 0xff;

    memset(pkt.targethwaddr, 0, sizeof(pkt.targethwaddr));


    lock(&ifc->lock);

    pipe_set_flag(ifc->fd, F_ETH_PACKET_TYPE, 0x0806);
    pipe_set_flag(ifc->fd, F_DEST_MAC, UINT64_C(0xffffffffffff));

    stream_send(ifc->fd, &pkt, sizeof(pkt), O_BLOCKING);

    pipe_set_flag(ifc->fd, F_ETH_PACKET_TYPE, 0x0800);

    unlock(&ifc->lock);


    while (reply_ip != ip)
        yield();


    unlock(&arp_lock);

    return reply_mac;
}


void arp_incoming(struct interface *ifc)
{
    size_t sz = pipe_get_flag(ifc->fd, F_PRESSURE);

    struct arp_packet pkt;

    if (sz < sizeof(pkt))
    {
        pipe_set_flag(ifc->fd, F_FLUSH, 1);
        return;
    }

    stream_recv(ifc->fd, &pkt, sizeof(pkt), O_BLOCKING);

    if ((ntohs(pkt.hwaddrtype) != 0x0001) || (ntohs(pkt.protoaddrtype) != 0x0800))
        return;

    switch (ntohs(pkt.operation))
    {
        case 1:
        {
            if (ntohl(pkt.targetprotoaddr) == ifc->ip)
                return;

            uint64_t srcmac = pipe_get_flag(ifc->fd, F_MY_MAC);

            struct arp_packet res = {
                .hwaddrtype = htons(0x0001),
                .protoaddrtype = htons(0x0800),
                .hwaddrlen = 6,
                .protoaddrlen = 4,
                .operation = htons(2),
                .senderprotoaddr = htonl(ifc->ip),
                .targetprotoaddr = pkt.senderprotoaddr
            };

            for (int i = 0; i < 6; i++)
                res.senderhwaddr[i] = (srcmac >> (i * 8)) & 0xff;

            memcpy(res.targethwaddr, pkt.senderhwaddr, sizeof(pkt.targethwaddr));


            uint64_t destmac = 0;

            for (int i = 0; i < 6; i++)
                destmac = destmac | ((uint64_t)pkt.senderhwaddr[i] << (i * 8));


            // Nicht Interface locken, weil das der IP-Receive-Handler schon getan hat
            pipe_set_flag(ifc->fd, F_ETH_PACKET_TYPE, 0x0806);
            pipe_set_flag(ifc->fd, F_DEST_MAC, destmac);

            stream_send(ifc->fd, &res, sizeof(res), O_NONBLOCK);

            pipe_set_flag(ifc->fd, F_ETH_PACKET_TYPE, 0x0800);

            break;
        }

        case 2:
            reply_mac = 0;
            for (int i = 0; i < 6; i++)
                reply_mac = reply_mac | ((uint64_t)pkt.senderhwaddr[i] << (i * 8));

            reply_ip = ntohl(pkt.senderprotoaddr);
    }
}
