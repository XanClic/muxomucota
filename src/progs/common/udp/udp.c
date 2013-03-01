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


struct udp_header
{
    uint16_t src_port, dest_port;
    uint16_t length;
    uint16_t checksum;
} cc_packed;


struct checksum_header
{
    uint32_t src_ip, dest_ip;
    uint8_t zero, proto_type;
    uint16_t udp_length;
    struct udp_header udp;
} cc_packet;


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
            return 0;

        dest_ip = htonl(ip);

        char *end;
        int val = strtol(relpath, &end, 10);
        relpath = end;

        if ((val < 0x0000) || (val > 0xffff) || *relpath)
            return 0;

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


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*f));

    memcpy(nf, f, sizeof(*nf));

    nf->port = get_user_port();
    nf->signal = false;

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

    rwl_lock_r(&connection_lock);

    struct vfs_file **fp;
    for (fp = &connections; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

    rwl_require_w(&connection_lock);

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

    struct checksum_header *udph = malloc(sizeof(*udph) + size);

    udph->src_ip     = htonl(pipe_get_flag(ip_fd, F_MY_IP));
    udph->dest_ip    = htonl(f->dest_ip);
    udph->zero       = 0x00;
    udph->proto_type = 0x11;
    udph->udp_length = htons(sizeof(udph->udp) + size);

    udph->udp.src_port  = htons(f->port);
    udph->udp.dest_port = htons(f->dest_port);
    udph->udp.length    = htons(sizeof(udph->udp) + size);
    udph->udp.checksum  = 0x0000;

    memcpy(udph + 1, data, size);

    udph->udp.checksum = htons(calculate_network_checksum(udph, sizeof(*udph) + size));


    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, f->dest_ip);

    size_t sent = stream_send(ip_fd, &udph->udp, sizeof(udph->udp) + size, flags);

    unlock(&ip_lock);


    free(udph);


    if (flags & O_NONBLOCK)
        return size;

    return (sent > sizeof(udph->udp)) ? (sent - sizeof(udph->udp)) : 0;
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

    return -EINVAL;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ((interface == I_FS) || (interface == I_UDP) || (interface == I_SIGNAL));
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
        struct checksum_header *udph = malloc(total_length + offsetof(struct checksum_header, udp));

        stream_recv(fd, &udph->udp, total_length, O_BLOCKING);


        size_t raw_data_size = ntohs(udph->udp.length) - sizeof(udph->udp);


        udph->src_ip     = htonl(src_ip);
        udph->dest_ip    = htonl(dest_ip);
        udph->zero       = 0x00;
        udph->proto_type = 0x11;
        udph->udp_length = htons(total_length);


        // FIXME: Da hab ichs mir aber ein bisschen einfach gemacht (apropos 0xffff)
        if (udph->udp.checksum && (udph->udp.checksum != 0xffff) && calculate_network_checksum(udph, raw_data_size + sizeof(*udph)))
        {
            fprintf(stderr, "(udp) incoming checksum error, discarding packet\n");
            free(udph);
            continue;
        }


        rwl_lock_r(&connection_lock);

        for (struct vfs_file *f = connections; f != NULL; f = f->next)
        {
            if (ntohs(udph->udp.dest_port) != f->port)
                continue;


            struct packet *p = malloc(sizeof(*p));

            p->next     = NULL;

            p->src_ip   = src_ip;
            p->src_port = ntohs(udph->udp.src_port);

            p->length   = raw_data_size;
            p->data     = malloc(p->length);

            memcpy(p->data, udph + 1, p->length);


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


        free(udph);
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
