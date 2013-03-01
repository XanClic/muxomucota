#include <assert.h>
#include <compiler.h>
#include <drivers.h>
#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "arp.h"
#include "interfaces.h"
#include "routing.h"


struct ip_header
{
    uint8_t hdrlen_version;
    uint8_t priority;
    uint16_t length;
    uint16_t id;
    uint16_t frag_ofs_flags;
    uint8_t ttl;
    uint8_t proto_type;
    uint16_t checksum;
    uint32_t src_ip, dest_ip;
} cc_packed;


struct packet
{
    struct packet *next;

    uint32_t src, dest;
    int protocol;
    int ttl;

    size_t length;
    void *data;
};


static struct vfs_file
{
    bool ifc_fixed;

    struct interface *ifc;
    uint64_t mac;

    uint32_t dest;
    int protocol;
    int ttl;

    pid_t owner;
    bool signal;

    struct vfs_file *next;

    struct packet *inqueue;
    rw_lock_t inqueue_lock;
} *connections;

static rw_lock_t connection_lock = RW_LOCK_INITIALIZER;


static uint16_t calculate_network_checksum(void *buffer, int size)
{
    if (size < 2)
        return 0;

    bool odd = size & 1;
    size &= ~1;

    uint8_t *data = buffer;

    uint32_t sum = 0;
    for (int i = 0; i < size; i += 2)
        sum += ((data[i] << 8) & 0xff00) + (data[i + 1] & 0xff);

    if (odd)
        sum += (data[size] << 8) & 0xff00;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (~sum) & 0xffff;
}


static int64_t get_ip(const char *s)
{
    uint32_t ip = 0;

    for (int i = 0; i < 4; i++)
    {
        char *end;
        int val = strtol(s, &end, 10);
        s = end;

        if ((val < 0) || (val > 255) || ((*s != '.') && (i < 3)) || (*s && (i == 3)))
            return -1;

        ip = (ip << 8) | val;
        s++;
    }

    return ip;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)flags;

    struct vfs_file *f = malloc(sizeof(*f));

    f->ifc_fixed = false;

    uint32_t ip = 0;

    if (*relpath)
    {
        int64_t rip = get_ip(relpath + 1);

        if (rip < 0)
        {
            struct interface *i = locate_interface(relpath + 1);

            if (i == NULL)
                return 0;

            f->ifc = i;
            f->ifc_fixed = true;

            f->mac = 0;
        }
        else
        {
            ip = rip;
            find_route(ip, &f->ifc, &f->mac, NULL);
        }
    }
    else
    {
        f->ifc = NULL;
        f->mac = 0;
    }

    f->dest = ip;
    f->protocol = 0x01;
    f->ttl = 128;

    f->owner = -1;
    f->signal = false;

    f->inqueue = NULL;
    rwl_lock_init(&f->inqueue_lock);


    rwl_lock_w(&connection_lock);

    f->next = connections;
    connections = f;

    rwl_unlock_w(&connection_lock);


    return (uintptr_t)f;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*f));

    memcpy(nf, f, sizeof(*nf));


    rwl_lock_init(&nf->inqueue_lock);
    rwl_lock_r(&f->inqueue_lock);

    struct packet **endp = &nf->inqueue;

    for (struct packet *i = f->inqueue; i != NULL; i = i->next)
    {
        *endp = malloc(sizeof(*i));
        memcpy(*endp, i, sizeof(*i));

        (*endp)->data = malloc(i->length);
        memcpy((*endp)->data, i->data, i->length);

        endp = &(*endp)->next;
    }

    *endp = NULL;

    rwl_unlock_r(&f->inqueue_lock);


    rwl_lock_w(&connection_lock);

    nf->next = connections;
    connections = nf;

    rwl_unlock_w(&connection_lock);


    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;


    struct packet *p = f->inqueue;

    while (p != NULL)
    {
        struct packet *np = p->next;

        free(p->data);
        free(p);

        p = np;
    }


    rwl_lock_r(&connection_lock);

    struct vfs_file **fp;
    for (fp = &connections; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

    rwl_require_w(&connection_lock);

    if (*fp != NULL)
        *fp = (*fp)->next;

    rwl_unlock_w(&connection_lock);


    free(f);
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    struct vfs_file *f = (struct vfs_file *)id;


    struct packet *p;

    do
    {
        rwl_lock_w(&f->inqueue_lock);

        p = f->inqueue;
        if (p != NULL)
            f->inqueue = p->next;

        rwl_unlock_w(&f->inqueue_lock);

        if ((p == NULL) && !(flags & O_NONBLOCK))
            yield();
    }
    while ((p == NULL) && !(flags & O_NONBLOCK));

    if (p == NULL)
        return 0;


    size_t copy_sz = (p->length < size) ? p->length : size;

    memcpy(data, p->data, copy_sz);

    free(p->data);
    free(p);


    return copy_sz;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    struct vfs_file *f = (struct vfs_file *)id;

    if (f->ifc == NULL)
        return 0;


    struct ip_header *iph = malloc(sizeof(*iph) + size);

    iph->hdrlen_version = 0x45;
    iph->priority       = 0x00;
    iph->length         = htons(sizeof(*iph) + size);
    iph->id             = 0x0000;
    iph->frag_ofs_flags = htons(0x4000);
    iph->ttl            = f->ttl;
    iph->proto_type     = f->protocol;
    iph->checksum       = 0x0000;
    iph->src_ip         = htonl(f->ifc->ip);
    iph->dest_ip        = htonl(f->dest);

    iph->checksum = htons(calculate_network_checksum(iph, sizeof(*iph)));

    memcpy(iph + 1, data, size);


    lock(&f->ifc->lock);

    pipe_set_flag(f->ifc->fd, F_DEST_MAC, f->mac);

    size_t sent = stream_send(f->ifc->fd, iph, sizeof(*iph) + size, flags);

    unlock(&f->ifc->lock);


    free(iph);


    if (flags & O_NONBLOCK)
        return size;

    return (sent > sizeof(*iph)) ? (sent - sizeof(*iph)) : 0;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return (f->inqueue != NULL) ? f->inqueue->length : 0;

        case F_READABLE:
            return f->inqueue != NULL;

        case F_WRITABLE:
            return true;

        case F_IPC_SIGNAL:
            return f->signal;

        case F_DEST_IP:
            return (f->inqueue != NULL) ? f->inqueue->dest : 0;

        case F_SRC_IP:
            return (f->inqueue != NULL) ? f->inqueue->src : 0;

        case F_MY_IP:
            return (f->ifc != NULL) ? f->ifc->ip : 0;

        case F_IP_TTL:
            return (f->inqueue != NULL) ? f->inqueue->ttl : 0;

        case F_IP_PROTO_TYPE:
            return (f->inqueue != NULL) ? f->inqueue->protocol : 0;
    }

    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)value;

    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_FLUSH:
            if (value != 1)
                return 0;

            rwl_lock_w(&f->inqueue_lock);

            struct packet *p = f->inqueue;
            if (p != NULL)
                f->inqueue = p->next;

            rwl_unlock_w(&f->inqueue_lock);

            free(p->data);
            free(p);

            return 0;

        case F_IPC_SIGNAL:
            f->owner  = getpgid(getppid());
            f->signal = (bool)value;
            return 0;

        case F_DEST_IP:
            f->dest = value;
            return find_route(value, &f->ifc, &f->mac, f->ifc_fixed ? f->ifc : NULL) ? 0 : -EHOSTUNREACH;

        case F_MY_IP:
            if (f->ifc == NULL)
                return -ENOTCONN;

            f->ifc->ip = value;
            return 0;

        case F_IP_TTL:
            f->ttl = value;
            return 0;

        case F_IP_PROTO_TYPE:
            f->protocol = value;
            return 0;
    }

    return -EINVAL;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ((interface == I_FS) || (interface == I_IP) || (interface == I_SIGNAL));
}


static uintmax_t incoming(void)
{
    struct
    {
        pid_t pid;
        uintptr_t id;
    } pipe_info;

    size_t recv = popup_get_message(&pipe_info, sizeof(pipe_info));

    assert(recv == sizeof(pipe_info));


    int fd = find_pipe(pipe_info.pid, pipe_info.id);

    assert(fd >= 0);


    struct interface *i = find_interface(fd);

    if (i == NULL)
        return 0;


    lock(&i->lock);


    while (pipe_get_flag(fd, F_READABLE))
    {
        int packet_type = pipe_get_flag(fd, F_ETH_PACKET_TYPE);

        if (packet_type != 0x0800)
        {
            if (packet_type == 0x0806)
                arp_incoming(i);
            else
                pipe_set_flag(fd, F_FLUSH, 1);

            continue;
        }


        size_t total_length = pipe_get_flag(fd, F_PRESSURE);
        struct ip_header *iph = malloc(total_length);

        stream_recv(fd, iph, total_length, O_BLOCKING);


        if ((iph->dest_ip != 0xffffffff) && (ntohl(iph->dest_ip) != i->ip))
        {
            free(iph);
            continue;
        }

        if (calculate_network_checksum(iph, (iph->hdrlen_version & 0xf) * 4))
        {
            fprintf(stderr, "(ip) incoming checksum error, discarding packet\n");
            free(iph);
            continue;
        }


        rwl_lock_r(&connection_lock);

        for (struct vfs_file *f = connections; f != NULL; f = f->next)
        {
            if ((f->dest != ntohl(iph->src_ip)) && (f->dest != 0xffffffff))
                continue;


            struct packet *p = malloc(sizeof(*p));

            p->next     = NULL;

            p->src      = ntohl(iph->src_ip);
            p->dest     = ntohl(iph->dest_ip);
            p->protocol = iph->proto_type;
            p->ttl      = iph->ttl;

            p->length   = ntohs(iph->length) - (iph->hdrlen_version & 0xf) * 4;
            p->data     = malloc(p->length);

            memcpy(p->data, (void *)((uintptr_t)iph + (iph->hdrlen_version & 0xf) * 4), p->length);


            rwl_lock_r(&f->inqueue_lock);

            struct packet **pp;
            for (pp = &f->inqueue; *pp != NULL; pp = &(*pp)->next);

            rwl_require_w(&f->inqueue_lock);

            *pp = p;

            rwl_unlock_w(&f->inqueue_lock);


            if (f->signal)
            {
                struct
                {
                    pid_t pid;
                    uintptr_t id;
                } msg = {
                    .pid = getpgid(0),
                    .id = (uintptr_t)f
                };

                ipc_message(f->owner, IPC_SIGNAL, &msg, sizeof(msg));
            }
        }

        rwl_unlock_r(&connection_lock);


        free(iph);
    }


    unlock(&i->lock);


    return 0;
}


int main(void)
{
    popup_message_handler(IPC_SIGNAL, incoming);

    register_routing_ipc();

    daemonize("ip", true);
}
