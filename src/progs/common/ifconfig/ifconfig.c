#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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


static int64_t get_ip(char *s)
{
    uint32_t ip = 0;

    for (int i = 0; i < 4; i++)
    {
        int val = strtol(s, &s, 10);
        if ((val < 0) || (val > 255) || ((*s != '.') && (i < 3)) || (*s && (i == 3)))
            return -1;

        ip = (ip << 8) | val;
        s++;
    }

    return ip;
}


static void print_if_info(const char *name)
{
    char *ethname, *ipname;

    asprintf(&ethname, "(eth)/%s", name);
    asprintf( &ipname,  "(ip)/%s", name);

    int ethfd = create_pipe(ethname, O_RDONLY), ipfd = create_pipe(ipname, O_RDONLY);

    free(ethname);
    free(ipname);

    if (ethfd < 0)
    {
        if (ipfd >= 0)
            destroy_pipe(ipfd, 0);

        perror(ethname);
        return;
    }

    if (ipfd < 0)
    {
        destroy_pipe(ethfd, 0);

        perror(ipname);
        return;
    }


    if (!pipe_implements(ethfd, I_ETHERNET))
    {
        destroy_pipe(ethfd, 0);
        destroy_pipe( ipfd, 0);

        fprintf(stderr, "%s: Is not an ethernet interface.\n", ethname);
        return;
    }

    if (!pipe_implements(ipfd, I_IP))
    {
        destroy_pipe(ethfd, 0);
        destroy_pipe( ipfd, 0);

        fprintf(stderr, "%s: Is not an IP interface.\n", ipname);
        return;
    }


    uint64_t mac = pipe_get_flag(ethfd, F_MY_MAC);
    uint32_t ip  = pipe_get_flag(ipfd,  F_MY_IP);


    destroy_pipe(ethfd, 0);
    destroy_pipe( ipfd, 0);


    printf("%s: inet ", name);
    print_ip(ip);
    printf("  ether ");
    print_mac(mac);
    printf("\n");
}


int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        if (argc == 2)
            print_if_info(argv[1]);
        else
        {
            int64_t ip = get_ip(argv[2]);

            if (ip < 0)
            {
                fprintf(stderr, "%s is not a valid IP address.\n", argv[2]);
                return 1;
            }


            char *ipname;
            asprintf(&ipname, "(ip)/%s", argv[1]);

            int ipfd = create_pipe(ipname, O_RDWR);

            free(ipname);

            if (ipfd < 0)
            {
                perror(ipname);
                return 1;
            }

            if (!pipe_implements(ipfd, I_IP))
            {
                fprintf(stderr, "%s: Is not an IP interface.\n", ipname);
                return 1;
            }

            pipe_set_flag(ipfd, F_MY_IP, ip);

            destroy_pipe(ipfd, 0);
        }
    }
    else
    {
        int fd = create_pipe("(eth)", O_RDONLY);

        if (fd < 0)
        {
            perror("(eth)");
            return 1;
        }

        size_t ifs_sz = pipe_get_flag(fd, F_PRESSURE);
        char ifs[ifs_sz];

        stream_recv(fd, ifs, ifs_sz, O_BLOCKING);

        destroy_pipe(fd, 0);

        for (int i = 0; ifs[i]; i += strlen(&ifs[i]) + 1)
        {
            print_if_info(&ifs[i]);
            putchar('\n');
        }
    }

    return 0;
}
