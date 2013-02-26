#include <compiler.h>
#include <ipc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <arpa/inet.h>


struct routing_entry
{
    void *rsvd;

    uint32_t dest, mask, gw;

    char iface[];
};


static void print_ip(const char *s, uint32_t ip)
{
    printf("%15s: ", s);

    for (int i = 0; i < 4; i++, ip <<= 8)
    {
        printf("%u", ip >> 24);
        if (i < 3)
            putchar('.');
    }

    putchar('\n');
}


static uint32_t get_ip(const uint8_t *src)
{
    uint32_t ip = 0;

    for (int i = 0; i < 4; i++)
        ip = (ip << 8) | src[i];

    return ip;
}


static void put_ip(uint8_t *dst, uint32_t ip)
{
    for (int i = 0; i < 4; i++, ip <<= 8)
        dst[i] = ip >> 24;
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: dhclient <interface>\n\n");
        fprintf(stderr, "The interface must be a file which exists in (eth), (ip) and (udp).\n");

        return 1;
    }


    char *ethname, *ipname, *udpname;

    asprintf(&ethname, "(eth)/%s", argv[1]);
    asprintf(&ipname,   "(ip)/%s", argv[1]);
    asprintf(&udpname, "(udp)/%s", argv[1]);


    int ethfd = create_pipe(ethname, O_RDWR);
    int  ipfd = create_pipe( ipname, O_RDWR);
    int udpfd = create_pipe(udpname, O_RDWR);


    if (ethfd < 0)
    {
        perror(ethname);
        return 1;
    }

    if (ipfd < 0)
    {
        perror(ipname);
        return 1;
    }

    if (udpfd < 0)
    {
        perror(udpname);
        return 1;
    }


    if (!pipe_implements(ethfd, I_ETHERNET))
    {
        fprintf(stderr, "%s: Not an ethernet interface.\n", ethname);
        return 1;
    }

    if (!pipe_implements(ipfd, I_IP))
    {
        fprintf(stderr, "%s: Not an IP interface.\n", ipname);
        return 1;
    }

    if (!pipe_implements(udpfd, I_UDP))
    {
        fprintf(stderr, "%s: Not a UDP interface.\n", udpname);
        return 1;
    }


    pipe_set_flag(ipfd, F_MY_IP, 0x00000000);


    struct dhcp_packet
    {
        uint8_t op, htype, hlen, hops;
        uint32_t xid;
        uint16_t secs, flags;
        uint32_t ciaddr, yiaddr, siaddr, giaddr;
        uint8_t chaddr[16];
        char sname[64];
        char file[128];
        uint32_t magic_cookie;
        uint8_t options[];
    } cc_packed;


    struct dhcp_packet *out = malloc(sizeof(*out) + 16);


    uint64_t mac = pipe_get_flag(ethfd, F_MY_MAC);

    for (int i = 0; i < 6; i++, mac >>= 8)
        out->chaddr[i] = mac & 0xff;

    memset(&out->chaddr[6], 0, sizeof(out->chaddr) - 6);

    out->htype = 1;
    out->hlen  = 6;

    out->xid = 0x2a2a2a2a;
    out->magic_cookie = htonl(0x63825363);

    out->secs  = 0;
    out->flags = htons(0x8000);

    out->ciaddr = 0x00000000;
    out->yiaddr = 0x00000000;
    out->siaddr = 0x00000000;
    out->giaddr = 0x00000000;

    memset(out->sname, 0, sizeof(out->sname));
    memset(out->file, 0, sizeof(out->file));

    out->hops = 0;

    out->op = 1;

    out->options[0] = 53; // DHCP message type
    out->options[1] =  1;
    out->options[2] =  1; // DHCPDISCOVER

    out->options[3] = 55; // parameter request list
    out->options[4] =  3;
    out->options[5] =  1; // subnet mask
    out->options[6] =  3; // router
    out->options[7] =  6; // DNS

    out->options[8] = 255;


    pipe_set_flag(udpfd, F_MY_PORT,   68);
    pipe_set_flag(udpfd, F_DEST_PORT, 67);
    pipe_set_flag(udpfd, F_DEST_IP,   0xffffffff);


    printf("Looking for DHCP server...\n");

    stream_send(udpfd, out, sizeof(*out) + 9, O_BLOCKING);


    while (!pipe_get_flag(udpfd, F_READABLE))
        yield();


    size_t insz = pipe_get_flag(udpfd, F_PRESSURE);

    struct dhcp_packet *in = malloc(insz);
    stream_recv(udpfd, in, insz, O_BLOCKING);


    printf("Got answer, checking... ");
    fflush(stdout);


    if (in->xid != 0x2a2a2a2a)
    {
        printf("Invalid XID\n");
        return 1;
    }

    if (in->op != 2)
    {
        printf("Not an answer\n");
        return 1;
    }


    printf("Seems OK\n");


    uint32_t my_addr = ntohl(in->yiaddr), server_addr = ntohl(in->siaddr), gw_addr = ntohl(in->giaddr);
    uint32_t subnet = 0xffffff00, router_addr = 0x00000000, dns_addr = 0x00000000;

    for (int i = 0; ((uintptr_t)&in->options[i] - (uintptr_t)in < insz) && (in->options[i] != 255); i += in->options[i + 1] + 2)
    {
        switch (in->options[i])
        {
            case 1: // subnet mask
                subnet = get_ip(&in->options[i + 2]);
                break;

            case 3: // router
                router_addr = get_ip(&in->options[i + 2]);
                break;

            case 6: // DNS
                dns_addr = get_ip(&in->options[i + 2]);
                break;

            case 53:
                if (in->options[i + 2] != 2) // DHCPOFFER
                {
                    printf("Ooops, actually is not.\n");
                    return 1;
                }
                break;
        }
    }


    free(in);


    print_ip("Proposed IP", my_addr);
    print_ip("Server IP", server_addr);
    print_ip("Gateway IP", gw_addr);
    print_ip("Router IP", router_addr);
    print_ip("DNS IP", dns_addr);
    print_ip("Subnet mask", subnet);


    out->siaddr = htonl(server_addr);

    out->options[ 0] = 53; // DHCP message type
    out->options[ 1] =  1;
    out->options[ 2] =  3; // DHCPREQUEST

    out->options[ 3] = 50; // IP request
    out->options[ 4] =  4;
    put_ip(&out->options[5], my_addr);

    out->options[ 9] = 54; // DHCP server
    out->options[10] =  4;
    put_ip(&out->options[11], server_addr);

    out->options[15] = 255;


    printf("Accepting IP...\n");

    stream_send(udpfd, out, sizeof(*out) + 16, O_BLOCKING);


    while (!pipe_get_flag(udpfd, F_READABLE))
        yield();


    insz = pipe_get_flag(udpfd, F_PRESSURE);

    in = malloc(insz);
    stream_recv(udpfd, in, insz, O_BLOCKING);


    printf("Got answer, checking... ");
    fflush(stdout);


    if (in->xid != 0x2a2a2a2a)
    {
        printf("Invalid XID\n");
        return 1;
    }

    if (in->op != 2)
    {
        printf("Not an answer\n");
        return 1;
    }


    for (int i = 0; ((uintptr_t)&in->options[i] - (uintptr_t)in < insz) && (in->options[i] != 255); i += in->options[i + 1] + 2)
    {
        switch (in->options[i])
        {
            case 53:
                if (in->options[i + 2] != 5) // DHCPACK
                {
                    printf("Not acknowledged\n");
                    return 1;
                }
                break;
        }
    }


    printf("OK\n");

    printf("Bringing interface up...\n");


    pipe_set_flag(ipfd, F_MY_IP, my_addr);


    pid_t pid = find_daemon_by_name("route");

    if (pid < 0)
    {
        fprintf(stderr, "Could not find routing service.\n");
        return 1;
    }


    size_t resz = sizeof(struct routing_entry) + strlen(argv[1]) + 1;
    struct routing_entry *re = malloc(resz);
    strcpy(re->iface, argv[1]);

    if (subnet)
    {
        re->dest = my_addr & subnet;
        re->mask = subnet;
        re->gw   = 0;

        ipc_message_synced(pid, 0, re, resz);
    }

    if (router_addr)
    {
        re->dest = 0;
        re->mask = 0;
        re->gw   = router_addr;

        ipc_message_synced(pid, 0, re, resz);
    }


    return 0;
}
