#include <stdio.h>
#include <vfs.h>


static void print_ip(uint32_t ip)
{
    for (int i = 0; i < 4; i++, ip <<= 8)
    {
        printf("%u", ip >> 24);
        if (i < 3)
            putchar('.');
    }
}


static void print_mac(uint64_t mac)
{
    for (int i = 0; i < 6; i++, mac >>= 8)
    {
        printf("%02x", (int)(mac & 0xff));
        if (i < 5)
            putchar(':');
    }
}


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: ifconfig <interface>\n\n");
        fprintf(stderr, "The interface must be a file present in (eth) and (ip).\n");

        return 1;
    }


    char *ethname, *ipname;

    asprintf(&ethname, "(eth)/%s", argv[1]);
    asprintf( &ipname,  "(ip)/%s", argv[1]);

    int ethfd = create_pipe(ethname, O_RDONLY), ipfd = create_pipe(ipname, O_RDONLY);

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


    if (!pipe_implements(ethfd, I_ETHERNET))
    {
        fprintf(stderr, "%s: Is not an ethernet interface.\n", ethname);
        return 1;
    }

    if (!pipe_implements(ipfd, I_IP))
    {
        fprintf(stderr, "%s: Is not an IP interface.\n", ipname);
        return 1;
    }


    uint64_t mac = pipe_get_flag(ethfd, F_MY_MAC);
    uint32_t ip  = pipe_get_flag(ipfd,  F_MY_IP);


    printf("%s: inet ", argv[1]);
    print_ip(ip);
    printf("  ether ");
    print_mac(mac);
    printf("\n");


    return 0;
}
