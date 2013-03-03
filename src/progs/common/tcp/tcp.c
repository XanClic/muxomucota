#include <assert.h>
#include <compiler.h>
#include <drivers.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system-timer.h>
#include <unistd.h>
#include <vfs.h>
#include <arpa/inet.h>


#define DEFAULT_WINDOW 16384U
#define RTT 1000
#define MSS 1300

#define TCP_FIN (1 << 0)
#define TCP_SYN (1 << 1)
#define TCP_RST (1 << 2)
#define TCP_PSH (1 << 3)
#define TCP_ACK (1 << 4)
#define TCP_URG (1 << 5)


struct tcp_header
{
    uint16_t src_port, dest_port;
    uint32_t seq, ack;
    uint8_t hdrlen;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgentptr;
} cc_packed;

struct pseudo_ip_header
{
    uint32_t src_ip, dest_ip;
    uint8_t zero, proto_type;
    uint16_t tcp_length;
} cc_packed;


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

    int last_time_sent;

    uint32_t seq_end;
    size_t read_offset;

    size_t size;
    struct tcp_header *packet;
};


struct tcp_connection
{
    uint32_t ip, dest_ip;
    uint16_t port, dest_port;

    volatile enum tcp_status status;

    uint32_t seq, ack;

    pid_t owner;
    bool signal;

    size_t window;

    struct tcp_connection *next;

    struct packet *inqueue;
    rw_lock_t inqueue_lock;

    struct packet *outqueue;
    rw_lock_t outqueue_lock;
};

static struct tcp_connection *connections;
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


static uint32_t raw_checksum_calc(const uint8_t *buffer, size_t sz, uint32_t initial)
{
    bool hi_byte = true;

    for (int i = 0; i < (int)sz; i++, hi_byte = !hi_byte)
        initial += (buffer[i] << (hi_byte * 8)) & (0xff << (hi_byte * 8));

    return initial;
}


static uint16_t calculate_network_checksum(const struct pseudo_ip_header *psip, const struct tcp_header *tcph, size_t sz)
{
    uint32_t sum = raw_checksum_calc((const uint8_t *)psip, sizeof(*psip), 0);
    sum = raw_checksum_calc((const uint8_t *)tcph, sz, sum);

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


static void throw_tcp_packet(struct tcp_connection *c, struct packet *p)
{
    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, c->dest_ip);
    stream_send(ip_fd, p->packet, p->size, O_NONBLOCK);

    unlock(&ip_lock);
}


static void handle_outqueue(struct tcp_connection *c)
{
    rwl_lock_r(&c->outqueue_lock);

    uint32_t seq_start = (c->outqueue != NULL) ? ntohl(c->outqueue->packet->seq) : 0;

    for (struct packet *p = c->outqueue; (p != NULL) && (p->seq_end - seq_start <= c->window); p = p->next)
        if (elapsed_ms() - p->last_time_sent > RTT)
            throw_tcp_packet(c, p);

    rwl_unlock_r(&c->outqueue_lock);
}


static void tcp_acked(struct tcp_connection *c, uint32_t ack)
{
    rwl_lock_w(&c->outqueue_lock);

    struct packet *p = c->outqueue;
    while ((p != NULL) && (ack - p->seq_end < 0x40000000U)) // p->seq_end <= ack
    {
        struct packet *np = p->next;

        free(p->packet);
        free(p);

        p = np;
    }

    c->outqueue = p;

    rwl_unlock_w(&c->outqueue_lock);
}


static void flush_all_queues(struct tcp_connection *c);
static void send_tcp_packet(struct tcp_connection *c, int flags, const void *data, size_t length);


static void handle_inqueue(struct tcp_connection *c)
{
    rwl_lock_w(&c->inqueue_lock);

    for (struct packet **pp = &c->inqueue; *pp != NULL; pp = &(*pp)->next)
    {
        struct packet *p = *pp;

        uint32_t seq = ntohl(p->packet->seq);

        if ((c->ack - seq < 0x40000000U) && // c->ack >= seq
            (c->ack != p->seq_end) &&
            (p->seq_end - c->ack < 0x40000000U)) // p->seq_end > c->ack
        {
            c->ack = p->seq_end;

            if (p->packet->flags & TCP_RST)
            {
                c->status = STATUS_CLOSED;
                flush_all_queues(c);
            }
            else if (p->packet->flags & TCP_SYN)
            {
                if (c->status == STATUS_LISTEN)
                {
                    c->status = STATUS_SYN_RCVD;
                    send_tcp_packet(c, TCP_SYN | TCP_ACK, NULL, 0);
                }
                else if (c->status == STATUS_CLOSED)
                    send_tcp_packet(c, TCP_RST, NULL, 0);
                else
                    c->status = STATUS_OPEN;
            }
            else if (p->packet->flags & TCP_FIN)
            {
                if ((c->status == STATUS_FIN_SENT) || (c->status == STATUS_CLOSED))
                    c->status = STATUS_CLOSED;
                else
                    c->status = STATUS_FIN_RCVD;
            }

            if (p->size == (p->packet->hdrlen >> 4) * 4)
            {
                *pp = p->next;

                free(p->packet);
                free(p);

                if (*pp == NULL)
                    break;
            }
        }
        else if (seq - c->ack < 0x40000000U) // seq >= c->ack (seq > c->ack)
            break;
    }

    rwl_unlock_w(&c->inqueue_lock);


    // TODO: Testen, ob am Ende der Queue schon ein ACK mit den richtigen Werten
    // hängt
    send_tcp_packet(c, TCP_ACK, NULL, 0);
}


static void send_tcp_packet(struct tcp_connection *c, int flags, const void *data, size_t length)
{
    struct packet *p = malloc(sizeof(*p));

    size_t seq_size = (!length && (flags & (TCP_SYN | TCP_FIN))) ? 1 : length;

    p->packet = malloc(sizeof(*p->packet) + length);

    uint32_t seq = __sync_fetch_and_add(&c->seq, seq_size);

    *p->packet = (struct tcp_header){
        .src_port = htons(c->port),
        .dest_port = htons(c->dest_port),
        .seq = htonl(seq),
        .ack = htonl(c->ack),
        .hdrlen = (sizeof(struct tcp_header) / 4) << 4,
        .flags = flags,
        .window = htons(DEFAULT_WINDOW),
        .checksum = 0x0000,
        .urgentptr = 0x0000
    };

    if (length)
        memcpy(p->packet + 1, data, length);

    p->seq_end = seq + seq_size;

    p->size = sizeof(*p->packet) + length;
    p->last_time_sent = 0;


    struct pseudo_ip_header psip = {
        .src_ip = htonl(c->ip),
        .dest_ip = htonl(c->dest_ip),
        .zero = 0x00,
        .proto_type = 0x06,
        .tcp_length = htons(p->size)
    };

    p->packet->checksum = htons(calculate_network_checksum(&psip, p->packet, p->size));


    rwl_lock_w(&c->outqueue_lock);

    struct packet **oq;
    for (oq = &c->outqueue; (*oq != NULL) && (ntohl((*oq)->packet->seq) <= seq); oq = &(*oq)->next);

    p->next = *oq;
    *oq = p;

    rwl_unlock_w(&c->outqueue_lock);


    handle_outqueue(c);
}


static void tcp_connect(struct tcp_connection *c)
{
    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, c->dest_ip);
    c->ip = pipe_get_flag(ip_fd, F_MY_IP);

    unlock(&ip_lock);


    flush_all_queues(c);


    c->seq = 0; // chosen by a fair dice roll (oder so)

    c->status = STATUS_SYN_SENT;
    send_tcp_packet(c, TCP_SYN, NULL, 0);

    int timeout = elapsed_ms() + 5000;
    while ((c->status == STATUS_SYN_SENT) && (elapsed_ms() < timeout))
        yield();
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

        dest_ip = ip;

        char *end;
        int val = strtol(relpath + 1, &end, 10);
        relpath = end;

        if ((val < 0x0000) || (val > 0xffff) || *relpath)
            return 0;

        dest_port = val;
    }


    struct tcp_connection *f = malloc(sizeof(*f));
    f->port      = get_user_port();
    f->dest_ip   = dest_ip;
    f->dest_port = dest_port;

    f->signal = false;
    f->owner  = -1;

    f->seq = f->ack = 0;

    f->window = DEFAULT_WINDOW;

    f->next = NULL;

    f->inqueue = NULL;
    rwl_lock_init(&f->inqueue_lock);

    f->outqueue = NULL;
    rwl_lock_init(&f->outqueue_lock);

    rwl_lock_w(&connection_lock);

    f->next = connections;
    connections = f;

    rwl_unlock_w(&connection_lock);


    if (!dest_ip)
        f->status = STATUS_CLOSED;
    else
    {
        tcp_connect(f);

        if (f->status != STATUS_OPEN)
        {
            service_destroy_pipe((uintptr_t)f, 0);
            return 0;
        }
    }


    return (uintptr_t)f;
}


static void flush_all_queues(struct tcp_connection *c)
{
    rwl_lock_w(&c->inqueue_lock);

    struct packet *p = c->inqueue;

    while (p != NULL)
    {
        struct packet *np = p->next;

        free(p->packet);
        free(p);

        p = np;
    }

    rwl_unlock_w(&c->inqueue_lock);


    rwl_lock_w(&c->outqueue_lock);

    p = c->inqueue;

    while (p != NULL)
    {
        struct packet *np = p->next;

        free(p->packet);
        free(p);

        p = np;
    }

    rwl_unlock_w(&c->outqueue_lock);
}


static void tcp_disconnect(struct tcp_connection *c)
{
    flush_all_queues(c);

    if ((c->status != STATUS_CLOSED) && (c->status != STATUS_LISTEN))
    {
        if ((c->status == STATUS_SYN_SENT) || (c->status == STATUS_SYN_RCVD))
        {
            send_tcp_packet(c, TCP_RST, NULL, 0);
            c->status = STATUS_CLOSED;
        }
        else
        {
            if (c->status == STATUS_OPEN)
                c->status = STATUS_FIN_SENT;
            else if (c->status == STATUS_FIN_RCVD)
                c->status = STATUS_CLOSED;

            send_tcp_packet(c, TCP_FIN | TCP_ACK, NULL, 0);

            int timeout = elapsed_ms() + RTT;
            while ((c->status != STATUS_CLOSED) && (elapsed_ms() < timeout))
                yield();

            if (c->status != STATUS_CLOSED)
            {
                send_tcp_packet(c, TCP_RST, NULL, 0);
                c->status = STATUS_CLOSED;
            }
        }
    }
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct tcp_connection *f = (struct tcp_connection *)id;


    tcp_disconnect(f);


    rwl_lock_r(&connection_lock);

    struct tcp_connection **fp;
    for (fp = &connections; (*fp != NULL) && (*fp != f); fp = &(*fp)->next);

    rwl_require_w(&connection_lock);

    if (*fp != NULL)
        *fp = f->next;

    rwl_unlock_w(&connection_lock);


    flush_all_queues(f);


    free(f);
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct tcp_connection *c = (struct tcp_connection *)id;

    if ((c->status != STATUS_OPEN) && (c->status != STATUS_FIN_RCVD))
        return 0;

    for (size_t i = 0; i < size; i += MSS)
    {
        size_t copy_sz = (size - i > MSS) ? MSS : (size - i);

        send_tcp_packet(c, TCP_ACK | ((i + MSS >= size) ? TCP_PSH : 0), (const char *)data + i, copy_sz);
    }

    return size;
}


static size_t tcp_receive(struct tcp_connection *c, void *data, size_t size)
{
    size_t orig_sz = size;
    char *out = data;

    rwl_lock_w(&c->inqueue_lock);

    uint32_t last_seq_end = (c->inqueue != NULL) ? ntohl(c->inqueue->packet->seq) : 0;

    for (struct packet **pp = &c->inqueue; *pp != NULL; pp = &(*pp)->next)
    {
        struct packet *p = *pp;

        if ((p->seq_end != c->ack) && (p->seq_end - c->ack < 0x40000000U)) // p->seq_end > c->ack
            break;

        uint32_t next_seq_end = p->seq_end;

        size_t data_size = p->size - (p->packet->hdrlen >> 4) * 4;
        if (data_size)
        {
            size_t remaining = data_size - p->read_offset - (last_seq_end - ntohl(p->packet->seq));
            size_t copy_sz = (remaining > size) ? size : remaining;

            if (copy_sz)
            {
                memcpy(out, (char *)p->packet + (p->packet->hdrlen >> 4) * 4, copy_sz);
                out += copy_sz;
                p->read_offset += copy_sz;
                size -= copy_sz;
            }

            if (data_size == p->read_offset + last_seq_end - ntohl(p->packet->seq))
            {
                *pp = p->next;

                free(p->packet);
                free(p);

                if (*pp == NULL)
                    break;
            }
        }

        last_seq_end = next_seq_end;
    }

    rwl_unlock_w(&c->inqueue_lock);


    return orig_sz - size;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    struct tcp_connection *c = (struct tcp_connection *)id;

    if (flags & O_NONBLOCK)
        return tcp_receive(c, data, size);
    else
    {
        big_size_t recvd = 0;
        do
        {
            recvd += tcp_receive(c, data, size);

            if ((recvd < size) && ((c->status == STATUS_OPEN) || (c->status == STATUS_FIN_SENT)))
                yield();
        }
        while ((recvd < size) && ((c->status == STATUS_OPEN) || (c->status == STATUS_FIN_SENT)));

        return recvd;
    }
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ((interface == I_TCP) || (interface == I_SIGNAL));
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct tcp_connection *c = (struct tcp_connection *)id;

    switch (flag)
    {
        case F_PRESSURE:
        {
            size_t press = 0;

            rwl_lock_r(&c->inqueue_lock);

            uint32_t last_seq_end = (c->inqueue != NULL) ? ntohl(c->inqueue->packet->seq) : 0;

            for (struct packet *p = c->inqueue; p != NULL; p = p->next)
            {
                if ((p->seq_end != c->ack) && (p->seq_end - c->ack < 0x40000000U)) // p->seq_end > c->ack
                    break;

                if (p->size > (p->packet->hdrlen >> 4) * 4)
                    press += p->seq_end - last_seq_end;

                last_seq_end = p->seq_end;
            }

            rwl_unlock_r(&c->inqueue_lock);

            return press;
        }

        case F_READABLE:
            return service_pipe_get_flag(id, F_PRESSURE) != 0;

        case F_WRITABLE:
            return (c->status == STATUS_OPEN) || (c->status == STATUS_FIN_RCVD);

        case F_IPC_SIGNAL:
            return c->signal;

        case F_DEST_IP:
        case F_SRC_IP:
            return c->dest_ip;

        case F_DEST_PORT:
            return c->dest_port;

        case F_SRC_PORT:
            // FIXME: Locking
            return (c->inqueue != NULL) ? ntohs(c->inqueue->packet->src_port) : c->dest_port;

        case F_MY_PORT:
            return c->port;

        case F_CONNECTION_STATUS:
            if (c->status == STATUS_CLOSED)
                return 0;
            else if (c->status == STATUS_OPEN)
                return 1;
            else if (c->status == STATUS_LISTEN)
                return 2;
            else
                return 3;
    }

    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct tcp_connection *c = (struct tcp_connection *)id;

    switch (flag)
    {
        case F_FLUSH:
            return 0;

        case F_IPC_SIGNAL:
            c->owner  = getpgid(getppid());
            c->signal = (bool)value;
            return 0;

        case F_DEST_IP:
            tcp_disconnect(c);
            c->dest_ip = value & 0xffffffffu;
            if (c->dest_ip && c->dest_port)
                tcp_connect(c);
            return 0;

        case F_DEST_PORT:
            tcp_disconnect(c);
            c->dest_port = value & 0xffff;
            if (c->dest_ip && c->dest_port)
                tcp_connect(c);
            return 0;

        case F_MY_PORT:
            tcp_disconnect(c);
            c->port = value & 0xffff;
            if (c->dest_ip && c->dest_port)
                tcp_connect(c);
            return 0;

        case F_CONNECTION_STATUS:
            if (!value)
                tcp_disconnect(c);
            else if (value == 2)
            {
                tcp_disconnect(c);
                c->status = STATUS_LISTEN;
            }
            return 0;
    }

    return -EINVAL;
}


static uintmax_t incoming(void)
{
    static lock_t incoming_lock = LOCK_INITIALIZER;

    struct
    {
        pid_t pid;
        uintptr_t id;
    } pipe_info;

    size_t recv = popup_get_message(&pipe_info, sizeof(pipe_info));

    assert(recv == sizeof(pipe_info));


    int fd = find_pipe(pipe_info.pid, pipe_info.id);

    assert(fd >= 0);


    lock(&incoming_lock);


    while (pipe_get_flag(fd, F_READABLE))
    {
        if (pipe_get_flag(fd, F_IP_PROTO_TYPE) != 0x06)
        {
            pipe_set_flag(fd, F_FLUSH, 1);
            continue;
        }


        uint32_t src_ip = pipe_get_flag(fd, F_SRC_IP);
        uint32_t dest_ip = pipe_get_flag(fd, F_DEST_IP);


        size_t total_length = pipe_get_flag(fd, F_PRESSURE);
        struct tcp_header *tcph = malloc(total_length);

        stream_recv(fd, tcph, total_length, O_BLOCKING);


        struct pseudo_ip_header psip = {
            .src_ip = htonl(src_ip),
            .dest_ip = htonl(dest_ip),
            .zero = 0x00,
            .proto_type = 0x06,
            .tcp_length = htons(total_length)
        };


        if (calculate_network_checksum(&psip, tcph, total_length))
        {
            fprintf(stderr, "(tcp) incoming checksum error, discarding packet\n");
            free(tcph);
            continue;
        }


        uint32_t seq = ntohl(tcph->seq);

        size_t data_sz = total_length - (tcph->hdrlen >> 4) * 4;
        size_t seq_size = (!data_sz && (tcph->flags & (TCP_SYN | TCP_FIN))) ? 1 : data_sz;


        rwl_lock_r(&connection_lock);

        for (struct tcp_connection *c = connections; c != NULL; c = c->next)
        {
            if (ntohs(tcph->dest_port) != c->port)
                continue;

            c->window = ntohs(tcph->window);


            if ((tcph->flags & TCP_SYN) && ((c->status == STATUS_LISTEN) || (c->status == STATUS_SYN_SENT)))
                c->ack = ntohl(tcph->seq);

            if (tcph->flags & TCP_ACK)
                tcp_acked(c, ntohl(tcph->ack));


            if (!data_sz && !(tcph->flags & (TCP_SYN | TCP_FIN | TCP_RST)))
                continue;


            struct packet *p = malloc(sizeof(*p));

            p->seq_end = ntohl(tcph->seq) + seq_size;
            p->size = total_length;
            p->read_offset = 0;

            p->packet = malloc(total_length);
            memcpy(p->packet, tcph, total_length);


            rwl_lock_w(&c->inqueue_lock);

            struct packet **iq;
            for (iq = &c->inqueue; (*iq != NULL) && (ntohl((*iq)->packet->seq) <= seq); iq = &(*iq)->next);

            p->next = *iq;
            *iq = p;

            rwl_unlock_w(&c->inqueue_lock);


            handle_inqueue(c);


            if (c->signal)
            {
                struct
                {
                    pid_t pid;
                    uintptr_t id;
                } msg = {
                    .pid = getpgid(0),
                    .id = (uintptr_t)c
                };

                ipc_message(c->owner, IPC_SIGNAL, &msg, sizeof(msg));
            }
        }

        rwl_unlock_r(&connection_lock);


        free(tcph);
    }


    unlock(&incoming_lock);


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
    pipe_set_flag(ip_fd, F_IP_PROTO_TYPE, 0x06);

    daemonize("tcp", true);
}
