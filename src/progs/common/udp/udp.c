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


struct vfs_file
{
    enum
    {
        TYPE_DIR,
        TYPE_IFC
    } type;

    union
    {
        struct
        {
            struct interface *ifc;

            uint32_t dest_ip;
            uint16_t port, dest_port;

            pid_t owner;
            bool signal;

            struct vfs_file *next;

            struct packet *inqueue;
            lock_t inqueue_lock;
        };

        struct
        {
            off_t pos;
            char *data;
            size_t sz;
        };
    };
};


static struct interface
{
    struct interface *next;

    lock_t lock;

    char *name;
    int fd;

    struct vfs_file *listeners;
    lock_t listener_lock;
} *interfaces = NULL;

static lock_t interface_lock = LOCK_INITIALIZER;


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


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (!*relpath)
    {
        if (flags & O_WRONLY)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;
        f->pos  = 0;

        f->sz = 1;

        lock(&interface_lock);

        for (struct interface *i = interfaces; i != NULL; i = i->next)
            f->sz += strlen(i->name) + 1;

        char *d = f->data = malloc(f->sz);

        for (struct interface *i = interfaces; i != NULL; i = i->next)
        {
            strcpy(d, i->name);
            d += strlen(i->name) + 1;
        }

        unlock(&interface_lock);

        *d = 0;

        return (uintptr_t)f;
    }


    uint32_t dest_ip = 0;
    uint16_t dest_port = 0;

    struct interface *i;

    if (strchr(relpath + 1, '/') == NULL)
    {
        lock(&interface_lock);
        for (i = interfaces; (i != NULL) && strcmp(i->name, relpath + 1); i = i->next);
        unlock(&interface_lock);

        if (i == NULL)
        {
            if (!(flags & O_CREAT))
                return 0;

            i = malloc(sizeof(*i));
            i->name = strdup(relpath + 1);
            i->fd   = -1;

            lock_init(&i->lock);

            i->listeners = NULL;
            lock_init(&i->listener_lock);

            lock(&interface_lock);

            i->next = interfaces;
            interfaces = i;

            unlock(&interface_lock);
        }
    }
    else
    {
        char duped[strlen(relpath)];
        strcpy(duped, relpath + 1);

        char *ifc_name = strtok(duped, "/");

        lock(&interface_lock);
        for (i = interfaces; (i != NULL) && strcmp(i->name, ifc_name); i = i->next);
        unlock(&interface_lock);

        if (i == NULL)
            return 0;

        char *cptr = strtok(NULL, "/");

        if (strtok(NULL, "/") != NULL)
            return 0;

        for (int j = 0; j < 4; j++)
        {
            int val = strtol(cptr, &cptr, 10);
            if ((val < 0x00) || (val > 0xff) || ((*cptr != '.') && (j < 3)) || ((*cptr && (*cptr != ':')) && (j == 3)))
                return 0;

            cptr++;

            dest_ip |= (uint32_t)val << ((3 - j) * 8);
        }

        dest_ip = htonl(dest_ip);

        if (*cptr == ':')
        {
            int val = strtol(cptr, &cptr, 10);
            if ((val < 0x0000) || (val > 0xffff) || *cptr)
                return 0;

            dest_port = val;
        }
    }


    struct vfs_file *f = malloc(sizeof(*f));
    f->type = TYPE_IFC;
    f->ifc  = i;

    f->port      = get_user_port();
    f->dest_ip   = dest_ip;
    f->dest_port = dest_port;

    f->signal = false;
    f->owner  = -1;

    f->next = NULL;

    f->inqueue = NULL;
    lock_init(&f->inqueue_lock);

    lock(&i->listener_lock);

    f->next = i->listeners;
    i->listeners = f;

    unlock(&i->listener_lock);

    return (uintptr_t)f;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*f));

    memcpy(nf->data, f->data, f->sz);

    if (f->type == TYPE_DIR)
    {
        nf->data = malloc(f->sz);
        memcpy(nf->data, f->data, f->sz);
    }
    else if (f->type == TYPE_IFC)
    {
        lock(&nf->ifc->listener_lock);

        nf->next = nf->ifc->listeners;
        nf->ifc->listeners = nf;

        unlock(&nf->ifc->listener_lock);

        lock_init(&nf->inqueue_lock);

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

        unlock(&f->inqueue_lock);
    }

    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);
    else if (f->type == TYPE_IFC)
    {
        lock(&f->ifc->listener_lock);

        struct vfs_file **fp;
        for (fp = &f->ifc->listeners; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

        if (*fp != NULL)
            *fp = f->next;

        unlock(&f->ifc->listener_lock);


        struct packet *p = f->inqueue;

        while (p != NULL)
        {
            struct packet *np = p->next;

            free(p->data);
            free(p);

            p = np;
        }
    }

    free(f);
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_DIR:
        {
            size_t copy_sz = (size < f->sz - (size_t)f->pos) ? size : (f->sz - (size_t)f->pos);

            memcpy(data, f->data + f->pos, copy_sz);

            f->pos += copy_sz;

            return copy_sz;
        }

        case TYPE_IFC:
        {
            lock(&f->inqueue_lock);

            struct packet *p = f->inqueue;
            if (p != NULL)
                f->inqueue = p->next;

            unlock(&f->inqueue_lock);


            size_t copy_sz = (p->length < size) ? p->length : size;

            memcpy(data, p->data, copy_sz);

            free(p->data);
            free(p);


            return copy_sz;
        }
    }

    return 0;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_DIR:
            return 0;

        case TYPE_IFC:
        {
            if ((flags & O_TRANS_NC) == F_MOUNT_FILE)
            {
                const char *name = data;

                if (!size || name[size - 1])
                    return 0;

                int fd = create_pipe(name, O_RDWR);

                if (fd < 0)
                    return 0;

                if (!pipe_implements(fd, I_IP))
                {
                    destroy_pipe(fd, 0);
                    return 0;
                }


                lock(&f->ifc->lock);

                if (f->ifc->fd >= 0)
                    destroy_pipe(f->ifc->fd, 0);

                f->ifc->fd = fd;

                pipe_set_flag(fd, F_IPC_SIGNAL, true);
                pipe_set_flag(fd, F_IP_PROTO_TYPE, 0x11);

                unlock(&f->ifc->lock);


                return size;
            }


            struct checksum_header *udph = malloc(sizeof(*udph) + size);

            udph->src_ip     = htonl(pipe_get_flag(f->ifc->fd, F_MY_IP));
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


            lock(&f->ifc->lock);

            pipe_set_flag(f->ifc->fd, F_DEST_IP, f->dest_ip);

            size_t sent = stream_send(f->ifc->fd, &udph->udp, sizeof(udph->udp) + size, flags);

            unlock(&f->ifc->lock);


            free(udph);


            if (flags & O_NONBLOCK)
                return size;

            return (sent > sizeof(udph->udp)) ? (sent - sizeof(udph->udp)) : 0;
        }
    }

    return 0;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            if (f->type == TYPE_DIR)
                return f->sz - f->pos;
            else if ((f->type == TYPE_IFC) && (f->inqueue != NULL))
                return f->inqueue->length;
            else
                return 0;

        case F_READABLE:
            return (f->type == TYPE_DIR) ? (f->sz > f->pos) : (f->inqueue != NULL);

        case F_WRITABLE:
            return (f->type == TYPE_IFC);

        case F_FILESIZE:
            return (f->type == TYPE_DIR) ? f->sz : 0;

        case F_POSITION:
            return (f->type == TYPE_DIR) ? f->pos : 0;

        case F_INODE:
        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
        case F_BLOCK_COUNT:
            return 0;

        case F_MODE:
            return (f->type == TYPE_DIR) ? (S_IFDIR | S_IFODEV | 0777) : (S_IFODEV | 0666);

        case F_NLINK:
        case F_BLOCK_SIZE:
            return 1;

        case F_IPC_SIGNAL:
            return (f->type == TYPE_IFC) ? f->signal : false;

        case F_DEST_IP:
            return (f->type == TYPE_IFC) ? f->dest_ip : 0;

        case F_SRC_IP:
            return ((f->type == TYPE_IFC) && (f->inqueue != NULL)) ? f->inqueue->src_ip : 0;

        case F_DEST_PORT:
            return (f->type == TYPE_IFC) ? f->dest_port : 0;

        case F_SRC_PORT:
            return ((f->type == TYPE_IFC) && (f->inqueue != NULL)) ? f->inqueue->src_port : 0;

        case F_MY_PORT:
            return (f->type == TYPE_IFC) ? f->port : 0;
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
            if ((f->type != TYPE_IFC) || (value != 1))
                return 0;

            lock(&f->inqueue_lock);

            struct packet *p = f->inqueue;
            if (p != NULL)
                f->inqueue = p->next;

            unlock(&f->inqueue_lock);

            free(p->data);
            free(p);

            return 0;

        case F_POSITION:
            if (f->type != TYPE_DIR)
                return -EINVAL;

            f->pos = (value > f->sz) ? f->sz : value;
            return 0;

        case F_IPC_SIGNAL:
            if (f->type != TYPE_IFC)
                return -EINVAL;

            f->owner  = getpgid(getppid());
            f->signal = (bool)value;
            return 0;

        case F_DEST_IP:
            if (f->type != TYPE_IFC)
                return -EINVAL;

            f->dest_ip = value;
            return 0;

        case F_DEST_PORT:
            if (f->type != TYPE_IFC)
                return -EINVAL;

            f->dest_port = value;
            return 0;

        case F_MY_PORT:
            if (f->type != TYPE_IFC)
                return -EINVAL;

            f->port = value;
            return 0;
    }

    return -EINVAL;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (interface)
    {
        case I_FILE:
            return (f->type == TYPE_DIR);

        case I_STATABLE:
            return true;

        case I_FS:
        case I_UDP:
        case I_SIGNAL:
            return (f->type == TYPE_IFC);
    }

    return false;
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


    lock(&interface_lock);

    struct interface *i;
    for (i = interfaces; (i != NULL) && (i->fd != fd); i++);

    unlock(&interface_lock);


    if (i == NULL)
        return 0;


    lock(&i->lock);


    while (pipe_get_flag(fd, F_READABLE))
    {
        if (pipe_get_flag(fd, F_IP_PROTO_TYPE) != 0x11)
        {
            pipe_set_flag(fd, F_FLUSH, 1);
            continue;
        }


        uint32_t src_ip = pipe_get_flag(fd, F_SRC_IP);


        size_t total_length = pipe_get_flag(fd, F_PRESSURE);
        struct checksum_header *udph = malloc(total_length + offsetof(struct checksum_header, udp));

        stream_recv(fd, &udph->udp, total_length, O_BLOCKING);


        size_t raw_data_size = ntohs(udph->udp.length) - sizeof(udph->udp);


        // FIXME: Da hab ichs mir aber ein bisschen einfach gemacht (apropos 0xffff)
        if (udph->udp.checksum && (udph->udp.checksum == 0xffff) && calculate_network_checksum(udph, raw_data_size + sizeof(*udph)))
        {
            fprintf(stderr, "(udp) incoming checksum error, discarding packet\n");
            free(udph);
            continue;
        }


        lock(&i->listener_lock);

        for (struct vfs_file *f = i->listeners; f != NULL; f = f->next)
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


            lock(&f->inqueue_lock);

            struct packet **pp;
            for (pp = &f->inqueue; *pp != NULL; pp = &(*pp)->next);

            *pp = p;

            unlock(&f->inqueue_lock);


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

        unlock(&i->listener_lock);


        free(udph);
    }


    unlock(&i->lock);


    return 0;
}


int main(void)
{
    popup_message_handler(IPC_SIGNAL, incoming);

    daemonize("udp", true);
}
