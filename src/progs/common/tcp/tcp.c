// FIXME: Der Code ist von 2010 und gehört dringend gefixt.

#include <assert.h>
#include <drivers.h>
#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <system-timer.h>
#include <vfs.h>
#include <arpa/inet.h>

#include "tcp.h"


#define MSS 1300

#define TCP_PKT_INV_DATA (1 << 0)
#define TCP_PKT_SYN      (1 << 1)
#define TCP_PKT_FIN      (1 << 2)
#define TCP_PKT_RST      (1 << 3)
#define TCP_PKT_SPC      (TCP_PKT_SYN | TCP_PKT_FIN | TCP_PKT_RST)


struct tcppkt
{
    struct tcppkt *next;
    size_t size, position;
    void *data;

    int pkt_type;

    uint32_t seq;

    struct tcpfhdr fhdr;
};

enum tcpstat
{
    TCP_CONNECTED = 0,
    TCP_LOCAL_CLOSED,
    TCP_REMOTE_CLOSED,
    TCP_CLOSED,
    TCP_CONNECT
};

enum tcpclose
{
    TCP_SINGLE_FIN,
    TCP_DOUBLE_FIN,
    TCP_RESET
};

struct tcpcon
{
    uint32_t dest_ip;
    uint16_t dport, sport;

    lock_t lock;

    volatile struct tcppkt *queue_start;
    volatile struct tcppkt *contiguous;
    volatile struct tcppkt *out_queue;

    volatile int got_fin_somewhere;

    struct tcpcon *next;

    volatile enum tcpstat status;
    uint32_t seq, ack, recv_seq;
};

static struct tcpcon *connections = NULL;
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


static void throw_packet(struct tcppkt *pkt, struct tcpcon *con)
{
    size_t size_to_send = sizeof(pkt->fhdr.tcphdr) + ((pkt->pkt_type & TCP_PKT_INV_DATA) ? 0 : pkt->size);
    pkt->fhdr.tcphdr.ack = htonl(con->ack);
    pkt->fhdr.tcphdr.checksum = 0;
    pkt->fhdr.tcphdr.checksum = htons(calculate_network_checksum(&pkt->fhdr, sizeof(pkt->fhdr.psiphdr) + size_to_send));

    lock(&ip_lock);

    pipe_set_flag(ip_fd, F_DEST_IP, con->dest_ip);

    stream_send(ip_fd, &pkt->fhdr.tcphdr, size_to_send, O_NONBLOCK);

    unlock(&ip_lock);
}

static void do_tcp_queue(struct tcpcon *con)
{
    lock(&con->lock);

    struct tcppkt *pkt = (struct tcppkt *)con->out_queue;

    while (pkt != NULL)
    {
        throw_packet(pkt, con);
        pkt = pkt->next;
    }

    unlock(&con->lock);
}

static void adapt_connection_state(struct tcpcon *con, int completed_first)
{
    lock(&con->lock);

    // Immer nur das erste Paket ansehen
    if (con->contiguous != NULL)
    {
        if (con->contiguous->pkt_type & TCP_PKT_SYN)
            con->status = TCP_CONNECTED;
        else if ((con->contiguous->pkt_type & TCP_PKT_FIN) && ((con->contiguous->pkt_type & TCP_PKT_INV_DATA) || completed_first))
            con->status |= TCP_REMOTE_CLOSED;
        if (con->contiguous->pkt_type & TCP_PKT_RST)
            con->status = TCP_CLOSED;

        if ((con->contiguous->pkt_type & TCP_PKT_SPC) && (con->contiguous->pkt_type & TCP_PKT_INV_DATA))
        {
            struct tcppkt *f = (struct tcppkt *)con->contiguous;
            con->contiguous = f->next;
            free(f);

            unlock(&con->lock);
            adapt_connection_state(con, 0); // FIXME: Stacküberlauf möglich
            lock(&con->lock);
        }
    }

    unlock(&con->lock);
}

static void tcp_send_packet(struct tcpcon *con, int flags, const void *data, size_t length, int queue);

static void ack(struct tcpcon *con)
{
    tcp_send_packet(con, TCP_ACK, NULL, 0, 0);
}

static void do_input_queue(struct tcpcon *con)
{
    lock(&con->lock);

    struct tcppkt **pkt = (struct tcppkt **)&con->queue_start;
    struct tcppkt **cont = (struct tcppkt **)&con->contiguous;

    while (*cont != NULL)
        cont = &(*cont)->next;

    if ((*pkt != NULL) && ((*pkt)->pkt_type & TCP_PKT_SYN))
        con->ack = (*pkt)->seq;

    uint_fast32_t seq = con->ack;
    int just_acks = 1;

    while ((*pkt != NULL) && ((*pkt)->seq == seq))
    {
        if (((*pkt)->fhdr.tcphdr.flags & ~TCP_ACK) || (*pkt)->size)
            just_acks = 0;

        seq += (*pkt)->size;
        *cont = *pkt;
        cont = &(*cont)->next;
        *pkt = NULL;
    }

    con->ack = seq;

    unlock(&con->lock);

    adapt_connection_state(con, 0);

    if ((con->status != TCP_CLOSED) && !just_acks)
        ack(con);
}

static void tcp_send_packet(struct tcpcon *con, int flags, const void *data, size_t length, int queue)
{
    struct tcppkt *pkt = calloc(1, sizeof(*pkt) + (length ? length : 1));

    pkt->data = &pkt->fhdr + 1;

    pkt->size = length;
    if (((flags & TCP_SYN) || (flags & TCP_FIN)) && !length)
    {
        pkt->size = 1;
        pkt->pkt_type |= TCP_PKT_INV_DATA;
    }
    pkt->pkt_type |= (flags & TCP_SYN) ? TCP_PKT_SYN : 0;
    pkt->pkt_type |= (flags & TCP_FIN) ? TCP_PKT_FIN : 0;
    pkt->pkt_type |= (flags & TCP_RST) ? TCP_PKT_RST : 0;

    pkt->seq = con->seq;
    con->seq += pkt->size;

    pkt->fhdr = (struct tcpfhdr){
        .psiphdr = {
            .src_ip = htonl(pipe_get_flag(ip_fd, F_MY_IP)),
            .dst_ip = htonl(con->dest_ip),
            .prot_id = 0x06,
            .tcp_length = htons(sizeof(pkt->fhdr.tcphdr) + length)
        },
        .tcphdr = {
            .src_port = htons(con->sport),
            .dst_port = htons(con->dport),
            .seq = htonl(pkt->seq), // ACK wird direkt beim Senden gesetzt
            .hdrlen = (sizeof(pkt->fhdr.tcphdr) / sizeof(uint32_t)) << 4,
            .flags = flags,
            .window = htons(MSS)
        }
    };

    memcpy(pkt->data, data, length);

    if (queue)
    {
        lock(&con->lock);

        struct tcppkt **last = (struct tcppkt **)&con->out_queue;
        while (*last != NULL)
            last = &(*last)->next;
        *last = pkt;

        unlock(&con->lock);

        do_tcp_queue(con);
    }
    else
    {
        throw_packet(pkt, con);
        free(pkt);
    }
}

static void tcp_connect(struct tcpcon *con)
{
    con->status = TCP_CONNECT;
    con->seq = 42;
    tcp_send_packet(con, TCP_SYN, NULL, 0, 1);

    int timeout = elapsed_ms() + 1000;
    while ((con->status == TCP_CONNECT) && (elapsed_ms() < timeout))
        yield();
    if (con->status == TCP_CONNECT)
        con->status = TCP_CLOSED;
}

uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)flags;

    struct tcpcon *ntcpc = calloc(1, sizeof(*ntcpc));
    ntcpc->sport = get_user_port();
    ntcpc->status = TCP_CLOSED;

    lock_init(&ntcpc->lock);

    rwl_lock_r(&connection_lock);

    struct tcpcon **last = &connections;
    while (*last != NULL)
        last = &(*last)->next;

    rwl_require_w(&connection_lock);

    *last = ntcpc;

    rwl_unlock_w(&connection_lock);

    if (*relpath)
    {
        char *end = (char *)relpath + 1;
        uint32_t ip = 0;

        for (int i = 0; i < 4; i++)
        {
            int ipa = strtol(end, &end, 10);
            if (ipa & ~0xFF)
            {
                service_destroy_pipe((uintptr_t)ntcpc, 0);
                errno = ENOENT;
                return 0;
            }
            if (((i < 3) && (*end != '.')) || ((i == 3) && (*end != ':')))
            {
                service_destroy_pipe((uintptr_t)ntcpc, 0);
                errno = ENOENT;
                return 0;
            }
            ip |= ipa << ((3 - i) * 8);
            end++;
        }

        int port = strtol(end, &end, 10);
        if (port & ~0xFFFF)
        {
            service_destroy_pipe((uintptr_t)ntcpc, 0);
            errno = ENOENT;
            return 0;
        }

        ntcpc->dest_ip = ip;
        ntcpc->dport = port;

        tcp_connect(ntcpc);

        if (ntcpc->status != TCP_CONNECTED)
        {
            service_destroy_pipe((uintptr_t)ntcpc, 0);
            errno = ECONNREFUSED;
            return 0;
        }
    }

    return (uintptr_t)ntcpc;
}

static void closeconn(struct tcpcon *con, enum tcpclose status)
{
    switch (status)
    {
        case TCP_SINGLE_FIN:
            if (con->status & TCP_LOCAL_CLOSED)
                return;

            tcp_send_packet(con, TCP_FIN | TCP_ACK, NULL, 0, 1);
            con->status |= TCP_LOCAL_CLOSED;
            break;

        case TCP_DOUBLE_FIN:
            if (con->status & TCP_CLOSED)
                return;

            con->got_fin_somewhere = 0;
            tcp_send_packet(con, TCP_FIN | TCP_ACK, NULL, 0, 1);
            con->status |= TCP_LOCAL_CLOSED;
            if (con->status == TCP_CLOSED)
                return;
            int timeout = elapsed_ms() + 1000;
            while ((elapsed_ms() < timeout) && !con->got_fin_somewhere)
                yield();
            if (con->got_fin_somewhere)
                con->status = TCP_CLOSED;
            break;

        case TCP_RESET:
            if (con->status & TCP_CLOSED)
                return;

            tcp_send_packet(con, TCP_RST, NULL, 0, 1);
            con->status = TCP_CLOSED;
    }
}

static void flush_all_queues(struct tcpcon *con)
{
    lock(&con->lock);

    struct tcppkt *pkt = (struct tcppkt *)con->queue_start;
    while (pkt != NULL)
    {
        struct tcppkt *next = pkt->next;
        free(pkt);
        pkt = next;
    }

    pkt = (struct tcppkt *)con->contiguous;
    while (pkt != NULL)
    {
        struct tcppkt *next = pkt->next;
        free(pkt);
        pkt = next;
    }

    pkt = (struct tcppkt *)con->out_queue;
    while (pkt != NULL)
    {
        struct tcppkt *next = pkt->next;
        free(pkt);
        pkt = next;
    }

    con->queue_start = con->contiguous = con->out_queue = NULL;

    unlock(&con->lock);
}

void service_destroy_pipe(uintptr_t id, int flags)
{
    struct tcpcon *tcpc = (struct tcpcon *)id;

    if (tcpc->status != TCP_CLOSED)
    {
        if (!flags)
            closeconn(tcpc, TCP_DOUBLE_FIN);
        if (tcpc->status != TCP_CLOSED)
            closeconn(tcpc, TCP_RESET);
    }

    rwl_lock_r(&connection_lock);

    struct tcpcon **prev = &connections;
    while ((*prev != NULL) && (*prev != tcpc))
        prev = &(*prev)->next;

    rwl_require_w(&connection_lock);

    if (*prev != NULL)
        *prev = tcpc->next;

    rwl_unlock_w(&connection_lock);

    flush_all_queues(tcpc);

    free(tcpc);
}

int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    static int has_both = 0;
    struct tcpcon *con = (struct tcpcon *)(uintptr_t)id;

    if (flag == F_DEST_PORT)
        has_both |= 1;
    else if (flag == F_DEST_IP)
        has_both |= 2;

    if (has_both == 3)
    {
        closeconn(con, TCP_DOUBLE_FIN);
        if (con->status != TCP_CLOSED)
            closeconn(con, TCP_RESET);

        flush_all_queues(con);
    }

    switch (flag)
    {
        case F_DEST_PORT:
            if (value & ~0xFFFF)
                return -EINVAL;

            con->dport = value;
            break;

        case F_DEST_IP:
            if (value & ~0xFFFFFFFFULL)
                return -EINVAL;

            con->dest_ip = value;
            break;

        case F_CONNECTION_STATUS:
            if (!value)
            {
                closeconn(con, TCP_DOUBLE_FIN);
                if (con->status != TCP_CLOSED)
                    closeconn(con, TCP_RESET);

                flush_all_queues(con);
            }
            break;
    }

    if (has_both == 3)
    {
        has_both = 0;

        tcp_connect(con);

        if (con->status != TCP_CONNECTED)
            return -ECONNREFUSED;

        return 0;
    }

    return -EINVAL;
}

uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct tcpcon *con = (struct tcpcon *)id;

    switch (flag)
    {
        case F_PRESSURE:
        {
            lock(&con->lock);

            size_t size = 0;

            struct tcppkt *pkt = (struct tcppkt *)con->contiguous;
            while (pkt != NULL)
            {
                if (!(pkt->pkt_type & TCP_PKT_INV_DATA))
                    size += pkt->size;
                pkt = pkt->next;
            }

            unlock(&con->lock);

            return size;
        }

        case F_READABLE:
            return (((con->status == TCP_CONNECTED) || (con->status == TCP_LOCAL_CLOSED)) && service_pipe_get_flag(id, F_PRESSURE));

        case F_WRITABLE:
            return ((con->status == TCP_CONNECTED) || (con->status == TCP_REMOTE_CLOSED));

        case F_DEST_PORT:
            return con->dport;

        case F_DEST_IP:
            return con->dest_ip;
    }

    errno = EINVAL;

    return 0;
}

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct tcpcon *con = (struct tcpcon *)id;
    size_t swas = size;

    // TODO: O_BLOCKING
    while (size)
    {
        size_t this_len = (size > MSS) ? MSS : size;

        tcp_send_packet(con, ((size > this_len) ? 0 : TCP_PSH) | TCP_ACK, data, this_len, 1);

        size -= this_len;
        data = (const void *)((uintptr_t)data + this_len);
    }

    return swas;
}

big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    struct tcpcon *con = (struct tcpcon *)(uintptr_t)id;
    size_t reced = 0;

    adapt_connection_state(con, 0);

    struct tcppkt *pkt = (struct tcppkt *)con->contiguous;

    if ((pkt == NULL) && !(flags & O_NONBLOCK) && size)
    {
        while (con->contiguous == NULL)
            yield();
        pkt = (struct tcppkt *)con->contiguous;
    }

    while (pkt != NULL)
    {
        int complete = 1;
        size_t this_len = pkt->size - pkt->position;

        if (this_len > size)
        {
            complete = 0;
            this_len = size;
        }

        memcpy(data, (const void *)((uintptr_t)pkt->data + pkt->position), this_len);
        reced += this_len;
        size -= this_len;

        if (!complete)
        {
            pkt->position += this_len;
            break;
        }
        else
        {
            adapt_connection_state(con, 1);
            struct tcppkt *next = pkt->next;
            con->contiguous = next;
            free(pkt);

            data = (void *)((uintptr_t)data + this_len);
        }

        pkt = (struct tcppkt *)con->contiguous;

        if ((pkt == NULL) && !(flags & O_NONBLOCK) && size)
        {
            while (con->contiguous == NULL)
                yield();
            pkt = (struct tcppkt *)con->contiguous;
        }
    }

    return reced;
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
        if (pipe_get_flag(fd, F_IP_PROTO_TYPE) != 0x06)
        {
            pipe_set_flag(fd, F_FLUSH, 1);
            continue;
        }

        size_t press = pipe_get_flag(fd, F_PRESSURE);

        struct tcppkt *pkt = calloc(1, sizeof(*pkt) + press);

        pkt->fhdr.psiphdr = (struct tcppsiphdr){
            .src_ip = htonl(pipe_get_flag(fd, F_SRC_IP)),
            .dst_ip = htonl(pipe_get_flag(fd, F_DEST_IP)),
            .prot_id = 0x06,
            .tcp_length = htons(press)
        };

        stream_recv(fd, &pkt->fhdr.tcphdr, press, O_BLOCKING);

        if (calculate_network_checksum(&pkt->fhdr, sizeof(pkt->fhdr.psiphdr) + press))
        {
            fprintf(stderr, "(tcp) incoming checksum error, discarding packet\n");
            free(pkt);
            continue;
        }

        pkt->data = (uint32_t *)&pkt->fhdr.tcphdr + (pkt->fhdr.tcphdr.hdrlen >> 4);
        pkt->size = press - sizeof(uint32_t) * (pkt->fhdr.tcphdr.hdrlen >> 4);
        if (!pkt->size && ((pkt->fhdr.tcphdr.flags & TCP_SYN) || (pkt->fhdr.tcphdr.flags & TCP_FIN) || (pkt->fhdr.tcphdr.flags & TCP_RST)))
        {
            pkt->size = 1;
            pkt->pkt_type |= TCP_PKT_INV_DATA;
        }
        pkt->pkt_type |= (pkt->fhdr.tcphdr.flags & TCP_SYN) ? TCP_PKT_SYN : 0;
        pkt->pkt_type |= (pkt->fhdr.tcphdr.flags & TCP_FIN) ? TCP_PKT_FIN : 0;
        pkt->pkt_type |= (pkt->fhdr.tcphdr.flags & TCP_RST) ? TCP_PKT_RST : 0;

        pkt->seq = ntohl(pkt->fhdr.tcphdr.seq);

        int used = 0;

        rwl_lock_r(&connection_lock);

        for (struct tcpcon *tcpc = connections; tcpc != NULL; tcpc = tcpc->next)
        {
            if (ntohs(pkt->fhdr.tcphdr.dst_port) != tcpc->sport)
                continue;

            if (pkt->seq < tcpc->ack)
                break;

            lock(&tcpc->lock);

            if (pkt->pkt_type & TCP_PKT_FIN)
                tcpc->got_fin_somewhere = 1;

            struct tcppkt **last = (struct tcppkt **)&tcpc->queue_start;
            while ((*last != NULL) && ((*last)->seq < pkt->seq)) // FIXME: Wrap-around
                last = &(*last)->next;

            if ((*last != NULL) && ((*last)->seq == pkt->seq))
            {
                unlock(&tcpc->lock);
                break;
            }

            pkt->next = *last;
            *last = pkt;

            if (pkt->fhdr.tcphdr.flags & TCP_ACK)
            {
                uint32_t ack_num = ntohl(pkt->fhdr.tcphdr.ack);

                struct tcppkt **outpkt = (struct tcppkt **)&tcpc->out_queue;
                while (*outpkt != NULL)
                {
                    if ((*outpkt)->seq < ack_num) // FIXME
                    {
                        struct tcppkt *this = *outpkt;
                        *outpkt = this->next;
                        free(this);
                    }
                }

                unlock(&tcpc->lock);
                do_tcp_queue(tcpc);
                lock(&tcpc->lock);
            }

            unlock(&tcpc->lock);

            used = 1;

            do_input_queue(tcpc);
        }

        rwl_unlock_r(&connection_lock);

        if (!used)
            free(pkt);
    }


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
