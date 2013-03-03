#include <drivers.h>
#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system-timer.h>
#include <vfs.h>

struct connection
{
    char *dname, *ip_string;
    uint32_t ip, dns_ip;
    int position;
};


static int proto_fd = -1;
static lock_t proto_lock = LOCK_INITIALIZER;
static bool use_tcp = false;

static uint32_t default_dns_ip = 0x08080808;


int main(void)
{
    proto_fd = create_pipe("(udp)", O_RDWR);

    if (proto_fd < 0)
    {
        proto_fd = create_pipe("(tcp)", O_RDWR);
        use_tcp = true;

        if (proto_fd < 0)
        {
            fprintf(stderr, "Could neither connect to the UDP nor the TCP service.\n");
            return 1;
        }
    }

    pipe_set_flag(proto_fd, F_DEST_PORT, 53);

    daemonize("dns", true);
}

uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (flags & O_WRONLY)
    {
        errno = EACCES;
        return 0;
    }

    struct connection *dnscon = calloc(1, sizeof(*dnscon));
    dnscon->dname = strdup(*relpath ? (relpath + 1) : "");
    dnscon->dns_ip = default_dns_ip;

    return (uintptr_t)dnscon;
}

void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct connection *dnsc = (struct connection *)id;

    if (dnsc->ip_string != NULL)
        free(dnsc->ip_string);
    free(dnsc->dname);
    free(dnsc);
}

uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct connection *dnsc = (struct connection *)id, *ndnsc = malloc(sizeof(*ndnsc));

    memcpy(ndnsc, dnsc, sizeof(*ndnsc));

    ndnsc->dname = strdup(dnsc->dname);
    ndnsc->ip_string = strdup(dnsc->ip_string);

    return (uintptr_t)ndnsc;
}

static int update_ip(struct connection *dnsc)
{
    if (dnsc->ip_string == NULL)
    {
        uint8_t *buf = malloc(2050);
        buf += 2;
        buf[ 0] = 0x00; //Query ID: 0x0000
        buf[ 1] = 0x00;
        buf[ 2] = 0x01; //Standard query (0x0100)
        buf[ 3] = 0x00;
        buf[ 4] = 0x00; //1 question
        buf[ 5] = 0x01;
        buf[ 6] = 0x00; //0 answer RRs
        buf[ 7] = 0x00;
        buf[ 8] = 0x00; //0 authority RRs
        buf[ 9] = 0x00;
        buf[10] = 0x00; //0 additional RRs
        buf[11] = 0x00;

        int i, j, o;
        for (i = 0, j = 0, o = 12; dnsc->dname[i]; i++)
        {
            if (dnsc->dname[i] == '.')
            {
                buf[o++] = i - j;
                for (; j < i; j++)
                    buf[o++] = dnsc->dname[j];
                j++;
            }
        }
        if (i - j)
        {
            buf[o++] = i - j;
            for (; j < i; j++)
                buf[o++] = dnsc->dname[j];
        }

        buf[o++] = 0;
        buf[o++] = 0x00; //Host address
        buf[o++] = 0x01;
        buf[o++] = 0x00; //Class: in
        buf[o++] = 0x01;

        lock(&proto_lock);

        pipe_set_flag(proto_fd, F_DEST_IP, dnsc->dns_ip);

        if (!use_tcp)
            stream_send(proto_fd, buf, o, O_BLOCKING);
        else
        {
            buf[-2] = o >> 8;
            buf[-1] = o & 0xFF;
            stream_send(proto_fd, buf - 2, o + 2, O_BLOCKING);
        }

        unlock(&proto_lock);

        int in = elapsed_ms();
        while ((elapsed_ms() - in < 5000) && !pipe_get_flag(proto_fd, F_READABLE))
            yield();

        if (!pipe_get_flag(proto_fd, F_READABLE))
        {
            free(buf - 2);
            return -ETIMEDOUT;
        }

        if (!use_tcp)
            stream_recv(proto_fd, buf, 2048, O_NONBLOCK);
        else
        {
            size_t pressure = pipe_get_flag(proto_fd, F_PRESSURE);
            stream_recv(proto_fd, buf - 2, pressure, O_NONBLOCK);
            size_t expected = (buf[-2] << 8) | buf[-1];

            pressure -= 2;

            if (pressure < expected)
                stream_recv(proto_fd, &buf[pressure], expected - pressure, O_BLOCKING);
        }

        if (use_tcp)
            pipe_set_flag(proto_fd, F_CONNECTION_STATUS, 0);

        o = 12;

        int qcount = (buf[4] << 8) | buf[5];
        int acount = (buf[6] << 8) | buf[7];

        for (i = 0; i < qcount; i++)
        {
            while (buf[o++]);
                o += 4;
        }

        for (i = 0; i < acount; i++)
        {
            o += 2;
            if (((buf[o] << 8) | buf[o + 1]) == 0x0001) //Host address
            {
                dnsc->ip_string = malloc(16);
                o += 10;
                sprintf(dnsc->ip_string, "%i.%i.%i.%i", buf[o], buf[o + 1], buf[o + 2], buf[o + 3]);

                dnsc->ip = 0;
                for (int k = 0; k < 4; k++)
                    dnsc->ip |= buf[o + k] << ((3 - k) * 8);

                break;
            }
            o += 8;
            o += ((buf[o] << 8) | buf[o + 1]) + 2; //Skip data and length field
        }

        free(buf - 2);
    }

    return 0;
}

big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct connection *dnsc = (struct connection *)id;

    errno = -update_ip(dnsc);

    if (errno)
        return 0;

    if (dnsc->ip_string == NULL)
        return 0;

    size_t len = strlen(dnsc->ip_string);
    if (size > len - dnsc->position)
        size = len - dnsc->position;

    if (!size)
        return 0;

    memcpy(data, &dnsc->ip_string[dnsc->position], size);
    dnsc->position += size;

    return size;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return (interface == I_DNS) || (interface == I_FILE);
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct connection *dnsc = (struct connection *)id;

    switch (flag)
    {
        case F_FLUSH:
            return 0;

        case F_POSITION:
            if (dnsc->ip_string == NULL)
                value = 0;
            else if (value > strlen(dnsc->ip_string))
                value = strlen(dnsc->ip_string);

            dnsc->position = value;
            return 0;

        case F_DNS_NSIP:
            if (value & ~0xFFFFFFFFULL)
                return -EINVAL;
            if (dnsc->ip_string != NULL)
            {
                free(dnsc->ip_string);
                dnsc->ip_string = NULL;
            }
            dnsc->dns_ip = value;
            return 0;

        case F_DNS_DEFAULT_NSIP:
            if (value & ~0xFFFFFFFFULL)
                return -EINVAL;
            default_dns_ip = value;
            return 0;
    }

    errno = EINVAL;
    return 0;
}

uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct connection *dnsc = (struct connection *)id;

    switch (flag)
    {
        case F_PRESSURE:
            update_ip(dnsc);
            return (dnsc->ip_string != NULL) ? (strlen(dnsc->ip_string) - dnsc->position) : 0;

        case F_READABLE:
            update_ip(dnsc);
            return (dnsc->ip_string != NULL) && (dnsc->position < (int)strlen(dnsc->ip_string));

        case F_WRITABLE:
            return 0;

        case F_FILESIZE:
            update_ip(dnsc);
            return (dnsc->ip_string != NULL) ? strlen(dnsc->ip_string) : 0;

        case F_POSITION:
            return dnsc->position;

        case F_DNS_RESOLVED_IP:
            return dnsc->ip;

        case F_DNS_NSIP:
            return dnsc->dns_ip;
    }

    errno = EINVAL;
    return 0;
}
