#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>
#include <system-timer.h>
#include <vfs.h>
#include <arpa/inet.h>

#include "nettools.h"

#define ICMP_ECHO_REPLY   0x00
#define ICMP_ECHO_REQUEST 0x08

struct icmp_pkt
{
    uint8_t cmd, code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seqnum;
} cc_packed;

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

static int calculate_network_checksum(const void *buffer, size_t size)
{
    uint_fast32_t sum = 0;
    const uint8_t *data = buffer;

    if ((buffer == NULL) || (size < 2))
        return 0;

    while (size > 1)
    {
        sum += (data[0] << 8) | data[1];
        data += 2;
        size -= 2;
    }
    if (size)
        sum += data[0] << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (~sum) & 0xFFFF;
}

noreturn static void help(int exitcode)
{
    printf("Usage: ping [-Vh?] [-c count] [-i interval] [-I interface] [-t ttl]\n");
    printf("            destination\n");
    exit(exitcode);
}

int main(int argc, char **argv)
{
    int count = 4, interval = 1000, ttl = 255;
    const char *dip = NULL;
    const char *ifc = NULL;

    for (int o = 1; o < argc; o++)
    {
        if (argv[o][0] == '-')
        {
            int cont = 1;
            for (int i = 1; argv[o][i] && cont; i++)
            {
                switch (argv[o][i])
                {
                    case 'c':
                        if (++o >= argc)
                        {
                            fprintf(stderr, "%s: Argument expected after -c.\n", argv[0]);
                            return 1;
                        }
                        count = atoi(argv[o]);

                        cont = 0;
                        continue;
                    case 'i':
                        if (++o >= argc)
                        {
                            fprintf(stderr, "%s: Argument expected after -i.\n", argv[0]);
                            return 1;
                        }
                        interval = strtod(argv[o], NULL) * 1000.0;

                        cont = 0;
                        continue;
                    case 'I':
                        if (++o >= argc)
                        {
                            fprintf(stderr, "%s: Argument expected after -I.\n", argv[0]);
                            return 1;
                        }
                        ifc = argv[o];

                        cont = 0;
                        continue;
                    case 't':
                        if (++o >= argc)
                        {
                            fprintf(stderr, "%s: Argument expected after -t.\n", argv[0]);
                            return 1;
                        }
                        ttl = atoi(argv[o]);
                        if ((ttl < 0) || (ttl > 255))
                        {
                            fprintf(stderr, "%s: Invalid TTL value.\n", argv[0]);
                            return 1;
                        }

                        cont = 0;
                        continue;
                    case 'h':
                    case '?':
                        help(0);
                    case 'V':
                        version(argv[0], 0);
                    default:
                        fprintf(stderr, "%s: Unknown option '%c'.\n", argv[0], argv[o][i]);
                        help(1);
                }
            }
        }
        else
        {
            if (dip == NULL)
                dip = argv[o];
            else
            {
                fprintf(stderr, "%s: Unexpected parameter \"%s\".\n", argv[0], argv[o]);
                return 1;
            }
        }
    }

    if (dip == NULL)
    {
        fprintf(stderr, "%s: Destination address required.\n", argv[0]);
        return 1;
    }

    int64_t dipv = get_ip(dip);

    if (dipv < 0)
    {
        fprintf(stderr, "%s: %s is not a valid IP address.\n", argv[0], dip);
        return 1;
    }

    const char *fname;
    int fd;

    if (ifc != NULL)
    {
        fname = malloc(22 + strlen(ifc));
        sprintf((char *)fname, "(ip)/%s", ifc);
        fd = create_pipe(fname, O_RDWR);
    }
    else
    {
        fname = "(ip)";
        fd = create_pipe("(ip)", O_RDWR);

        ifc = "routed";
    }

    if (pipe_set_flag(fd, F_DEST_IP, dipv) < 0)
    {
        fprintf(stderr, "%s: %s: %s\n", argv[0], dip, strerror(errno));
        return 1;
    }

    printf("Pinging %s via %s interface (file \"%s\")\n", dip, ifc, fname);

    if (fd < 0)
    {
        fprintf(stderr, "%s: Could not connect: %s\n", argv[0], strerror(errno));
        return 1;
    }

    pipe_set_flag(fd, F_IP_TTL, ttl);

    for (int i = 0; i < count; i++)
    {
        struct icmp_pkt icmp =
        {
            .cmd = ICMP_ECHO_REQUEST,
            .code = 0,
            .checksum = 0,
            .id = 0x2A42,
            .seqnum = htons(i)
        };

        icmp.checksum = htons(calculate_network_checksum(&icmp, sizeof(icmp)));

        pipe_set_flag(fd, F_IP_PROTO_TYPE, 0x01);

        int timeout = 0, diff;

        int ens = elapsed_ms();

        stream_send(fd, &icmp, sizeof(icmp), O_BLOCKING);

        for (;;)
        {
            while (pipe_get_flag(fd, F_PRESSURE) < sizeof(icmp))
            {
                if (elapsed_ms() - ens > 2000) // 2 s
                {
                    timeout = 1;
                    break;
                }

                yield();
            }

            diff = elapsed_ms() - ens;

            if (timeout)
            {
                printf("Timeout.\n");
                break;
            }

            ttl = pipe_get_flag(fd, F_IP_TTL);

            if (pipe_get_flag(fd, F_IP_PROTO_TYPE) != 0x01)
            {
                pipe_set_flag(fd, F_FLUSH, 1);
                continue;
            }

            stream_recv(fd, &icmp, sizeof(icmp), O_BLOCKING);

            if (calculate_network_checksum(&icmp, sizeof(icmp)))
                printf("Bad checksum.\n");
            else if ((icmp.cmd != ICMP_ECHO_REPLY) || icmp.code)
            {
                printf("ICMP reply: ");
                switch (icmp.cmd)
                {
                    case ICMP_ECHO_REQUEST:
                        printf("Echo request (PING).\n");
                        break;
                    case 3:
                        switch (icmp.code)
                        {
                            case 0:
                                printf("Network unreachable.\n");
                                break;
                            case 1:
                                printf("Host unreachable.\n");
                                break;
                            case 2:
                                printf("Protocol unreachable.\n");
                                break;
                            case 3:
                                printf("Port unreachable.\n");
                                break;
                            case 4:
                                printf("Must fragment though disallowed.\n");
                                break;
                            case 5:
                                printf("Route impossible.\n");
                                break;
                            case 13:
                                printf("Communication administratively prohibited.\n");
                                break;
                            default:
                                printf("Destination unreachable.\n");
                        }
                        break;
                    case 4:
                        printf("Destination queue unable to accept more packets.\n");
                        break;
                    case 11:
                        switch (icmp.code)
                        {
                            case 0:
                                printf("Time to live exceeded.\n");
                                break;
                            case 1:
                                printf("Time to defragment exceeded.\n");
                                break;
                            default:
                                printf("Time limit exceeded.\n");
                        }
                        break;
                    case 31:
                        printf("Datagram conversion error.\n");
                        break;
                    case 32:
                        printf("Mobile host redirect.\n");
                        break;
                    default:
                        printf("Unknown message type %i.\n", icmp.cmd);
                }
            }
            else
                printf("PONG: icmp_seq=%i id_correct=%i ttl=%i time=%i ms\n", (int)ntohs(icmp.seqnum), !!(icmp.id == 0x2A42), ttl, diff);

            if (pipe_get_flag(fd, F_PRESSURE) >= sizeof(icmp))
                continue;

            break;
        }

        if (i < count - 1)
            msleep(interval);
    }

    destroy_pipe(fd, 0);

    return 0;
}
