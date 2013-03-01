#include <drivers.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <arpa/inet.h>


#define DEFAULT_WINDOW 8192


enum tcp_status
{
    STATUS_CLOSED,
    STATUS_LISTEN,
    STATUS_SYN_RCVD,
    STATUS_SYN_SENT,
    STATUS_OPEN,
    STATUS_FIN_RCVD,
    STATUS_FIN_SENT
};


struct packet
{
    struct packet *next;

    uint32_t src_ip;
    uint16_t src_port;

    uint32_t seq;

    size_t length;
    void *data;
};


struct vfs_file
{
    uint32_t dest_ip;
    uint16_t port, dest_port;

    enum tcp_status status;

    uint32_t seq, ack;

    pid_t owner;
    bool signal;

    struct vfs_file *next;

    struct packet *inqueue;
    rw_lock_t inqueue_lock;

    void *outbuffer;
    size_t outbuffer_size, outbuffer_read_pos, outbuffer_write_pos;
    rw_lock_t outbuffer_lock;
};

static struct vfs_file *connections;
static rw_lock_t connection_lock = RW_LOCK_INITIALIZER;


static int ip_fd;
static lock_t ip_lock = LOCK_INITIALIZER;


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

        if ((val < 0) || (val > 255) || ((*s != '.') && (i < 3)) || (*s && (*s != ':') && (i == 3)))
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
    uint32_t dest_port = 0;

    if (*relpath)
    {
        int64_t ip = get_ip(relpath + 1, &relpath);

        if (ip < 0)
            return 0;

        dest_ip = htonl(ip);

        if (*relpath == ':')
        {
            char *end;
            int val = strtol(relpath, &end, 10);
            relpath = end;

            if ((val < 0x0000) || (val > 0xffff) || *relpath)
                return 0;

            dest_port = val;
        }
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

    f->outbuffer = malloc(DEFAULT_WINDOW);
    f->outbuffer_size = DEFAULT_WINDOW;
    f->outbuffer_read_pos = f->outbuffer_write_pos = 0;
    rwl_lock_init(&f->outbuffer_lock);

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

    rwl_lock_r(&connection_lock);

    struct vfs_file **fp;
    for (fp = &connections; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

    rwl_require_w(&connection_lock);

    if (*fp != NULL)
        *fp = f->next;

    rwl_unlock_w(&connection_lock);


    free(f->outbuffer);


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


static uintmax_t incoming(void)
{
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

    pipe_set_flag(ip_fd, F_IPC_SIGNAL, true);
    pipe_set_flag(ip_fd, F_IP_PROTO_TYPE, 0x06);

    daemonize("tcp", true);
}
