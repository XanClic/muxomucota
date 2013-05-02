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

#include <assert.h>
#include <compiler.h>
#include <drivers.h>
#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <shm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>


struct udp_header
{
    uint16_t src_port, dest_port;
    uint16_t length;
    uint16_t checksum;
} cc_packed;


struct pseudo_ip_header
{
    uint32_t src_ip, dest_ip;
    uint8_t zero, proto_type;
    uint16_t udp_length;
} cc_packed;


struct packet
{
    struct packet *next;

    uint32_t src_ip;
    uint16_t src_port;

    size_t length;
    void *data;
};


static lock_t ip_lock = LOCK_INITIALIZER;
static int ip_fd;


struct vfs_file
{
    uint32_t dest_ip;
    uint16_t port, dest_port;

    pid_t owner;
    bool signal;

    struct vfs_file *next;

    struct packet *inqueue;
    rw_lock_t inqueue_lock;
};

static struct vfs_file *connections;
static rw_lock_t connection_lock = RW_LOCK_INITIALIZER;


static uint16_t get_user_port(void)
{
    static uint16_t port_counter = 1024;
    static lock_t port_lock = LOCK_INITIALIZER;

    lock(&port_lock);

    uint16_t port;

    if (port_counter < 0xffff)
        port = port_counter++;
    else
    {
        port = port_counter;
        port_counter = 1024;
    }

    unlock(&port_lock);

    return port;
}


static uint32_t raw_checksum_calc(const uint8_t *buffer, size_t sz, uint32_t initial)
{
    bool hi_byte = true;

    for (int i = 0; i < (int)sz; i++, hi_byte = !hi_byte)
        initial += (buffer[i] << (hi_byte * 8)) & (0xff << (hi_byte * 8));

    return initial;
}


static uint16_t calculate_network_checksum(const struct pseudo_ip_header *psip, const struct udp_header *udph, size_t sz)
{
    uint32_t sum = raw_checksum_calc((const uint8_t *)psip, sizeof(*psip), 0);
    sum = raw_checksum_calc((const uint8_t *)udph, sz, sum);

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (~sum) & 0xffff;
}


static int64_t get_ip(const char *s, const char **endptr)
{
    uint32_t ip = 0;

    for (int i = 0; i < 4; i++)
    {
        char *end;
        int val = strtol(s, &end, 10);
        s = end;

        if ((val < 0) || (val > 255) || ((*s != '.') && (i < 3)) || ((*s != ':') && (i == 3)))
            return -1;

        ip = (ip << 8) | val;
        s++;
    }

    *endptr = s - 1;

    return ip;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)flags;

    uint32_t dest_ip = 0;
    uint16_t dest_port = 0;

    if (*relpath)
    {
        int64_t ip = get_ip(relpath + 1, &relpath);

        if (ip < 0)
        {
            errno = ENOENT;
            return 0;
        }

        dest_ip = htonl(ip);

        char *end;
        int val = strtol(relpath, &end, 10);
        relpath = end;

        if ((val < 0x0000) || (val > 0xffff) || *relpath)
        {
            errno = ENOENT;
            return 0;
        }

        dest_port = val;
    }


    struct vfs_file *f = malloc(sizeof(*f));
    f->port      = get_user_port();
    f->dest_ip   = dest_ip;
    f->dest_port = dest_port;

    f->signal = false;
    f->owner  = -1;

    f->next = NULL;

    f->inqueue = NULL;
    rwl_lock_init(&f->inqueue_lock);

    rwl_lock_w(&connection_lock);

    f->next = connections;
    connections = f;

    rwl_unlock_w(&connection_lock);

    return (uintptr_t)f;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    rwl_lock_w(&connection_lock);

    struct vfs_file **fp;
    for (fp = &connections; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

    if (*fp != NULL)
        *fp = f->next;

    rwl_unlock_w(&connection_lock);


    struct packet *p = f->inqueue;

    while (p != NULL)
    {
        struct packet *np = p->next;

        free(p->data);
        free(p);

        p = np;
    }

    free(f);
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

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

    uintptr_t udph_shm = shm_create(sizeof(struct udp_header) + size);
    struct udp_header *udph = shm_open(udph_shm, VMM_UW);

    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, f->dest_ip);
    uint32_t my_ip = pipe_get_flag(ip_fd, F_MY_IP);

    unlock(&ip_lock);

    struct pseudo_ip_header psip = {
        .src_ip     = htonl(my_ip),
        .dest_ip    = htonl(f->dest_ip),
        .zero       = 0x00,
        .proto_type = 0x11,
        .udp_length = htons(sizeof(*udph) + size)
    };

    udph->src_port  = htons(f->port);
    udph->dest_port = htons(f->dest_port);
    udph->length    = htons(sizeof(*udph) + size);
    udph->checksum  = 0x0000;

    memcpy(udph + 1, data, size);

    udph->checksum = htons(calculate_network_checksum(&psip, udph, sizeof(*udph) + size));


    if (flags & O_NONBLOCK)
        shm_close(udph_shm, udph, false);


    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, f->dest_ip);

    size_t sent = stream_send_shm(ip_fd, udph_shm, sizeof(*udph) + size, flags);

    unlock(&ip_lock);


    if (!(flags & O_NONBLOCK))
        shm_close(udph_shm, udph, true);


    return (sent > sizeof(*udph)) ? (sent - sizeof(*udph)) : 0;
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
            return f->dest_ip;

        case F_SRC_IP:
            return (f->inqueue != NULL) ? f->inqueue->src_ip : 0;

        case F_DEST_PORT:
            return f->dest_port;

        case F_SRC_PORT:
            return (f->inqueue != NULL) ? f->inqueue->src_port : 0;

        case F_MY_PORT:
            return f->port;
    }

    errno = EINVAL;
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
            f->dest_ip = value;
            return 0;

        case F_DEST_PORT:
            f->dest_port = value;
            return 0;

        case F_MY_PORT:
            f->port = value;
            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ((interface == I_UDP) || (interface == I_SIGNAL));
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


    lock(&ip_lock);


    while (pipe_get_flag(fd, F_READABLE))
    {
        if (pipe_get_flag(fd, F_IP_PROTO_TYPE) != 0x11)
        {
            pipe_set_flag(fd, F_FLUSH, 1);
            continue;
        }


        uint32_t src_ip = pipe_get_flag(fd, F_SRC_IP);
        uint32_t dest_ip = pipe_get_flag(fd, F_DEST_IP);


        size_t total_length = pipe_get_flag(fd, F_PRESSURE);

        uintptr_t udph_shm = shm_create(total_length);
        struct udp_header *udph = shm_open(udph_shm, VMM_UR | VMM_UW);

        stream_recv_shm(fd, udph_shm, total_length, O_BLOCKING);


        size_t raw_data_size = ntohs(udph->length) - sizeof(*udph);


        struct pseudo_ip_header psip = {
            .src_ip     = htonl(src_ip),
            .dest_ip    = htonl(dest_ip),
            .zero       = 0x00,
            .proto_type = 0x11,
            .udp_length = htons(total_length)
        };


        // FIXME: Da hab ichs mir aber ein bisschen einfach gemacht (apropos 0xffff)
        if (udph->checksum && (udph->checksum != 0xffff) && calculate_network_checksum(&psip, udph, total_length))
        {
            fprintf(stderr, "(udp) incoming checksum error, discarding packet\n");
            shm_close(udph_shm, udph, true);
            continue;
        }


        rwl_lock_r(&connection_lock);

        for (struct vfs_file *f = connections; f != NULL; f = f->next)
        {
            if (ntohs(udph->dest_port) != f->port)
                continue;


            struct packet *p = malloc(sizeof(*p));

            p->next     = NULL;

            p->src_ip   = src_ip;
            p->src_port = ntohs(udph->src_port);

            p->length   = raw_data_size;
            p->data     = malloc(p->length);

            memcpy(p->data, udph + 1, p->length);


            rwl_lock_w(&f->inqueue_lock);

            struct packet **pp;
            for (pp = &f->inqueue; *pp != NULL; pp = &(*pp)->next);

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


        shm_close(udph_shm, udph, true);
    }


    unlock(&ip_lock);


    return 0;
}


int main(void)
{
    popup_message_handler(IPC_SIGNAL, incoming);

    ip_fd = create_pipe("(ip)", O_RDWR);

    if (ip_fd < 0)
    {
        fprintf(stderr, "Could not connect to IP service.\n");
        return 1;
    }

    if (!pipe_implements(ip_fd, I_IP))
    {
        fprintf(stderr, "(ip) does not implement the IP interface.\n");
        return 1;
    }

    pipe_set_flag(ip_fd, F_IPC_SIGNAL, true);
    pipe_set_flag(ip_fd, F_IP_PROTO_TYPE, 0x11);

    daemonize("udp", true);
}
