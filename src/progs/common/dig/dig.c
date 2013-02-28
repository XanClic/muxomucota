#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>

int main(int argc, char *argv[])
{
    const char *domain = NULL;
    const char *dns_ip = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            fprintf(stderr, "%s: Unknown option '%c'.\n", argv[0], argv[i][0]);
            return 1;
        }
        else
        {
            if (argv[i][0] == '@')
            {
                if (dns_ip != NULL)
                {
                    fprintf(stderr, "%s: More than one DNS IP address specified.\n", argv[0]);
                    return 1;
                }
                dns_ip = &argv[i][1];
            }
            else if (domain == NULL)
                domain = argv[i];
            else
            {
                fprintf(stderr, "%s: Unexpected argument \"%s\".\n", argv[0], argv[i]);
                return 1;
            }
        }
    }

    if (domain == NULL)
    {
        fprintf(stderr, "%s: Lookup name required.\n", argv[0]);
        return 1;
    }

    uint_fast32_t dns_nip = 0;

    if (dns_ip != NULL)
    {
        char *end = (char *)dns_ip;
        int ipa[4];

        for (int i = 0; i < 4; i++)
        {
            ipa[i] = strtol(end, &end, 10);
            if (ipa[i] & ~0xFF)
            {
                fprintf(stderr, "%s: \"%s\" is no valid IP address.\n", argv[0], dns_ip);
                return 1;
            }
            if (((i < 3) && (*end != '.')) || ((i == 3) && *end))
            {
                fprintf(stderr, "%s: \"%s\" is no valid IP address.\n", argv[0], dns_ip);
                return 1;
            }
            dns_nip |= ipa[i] << ((3 - i) * 8);
            end++;
        }
    }

    char *fname;
    asprintf(&fname, "(dns)/%s", domain);

    int fd = create_pipe(fname, O_RDONLY);

    if (fd < 0)
    {
        fprintf(stderr, "%s: Could not open %s: %s\n", argv[0], fname, strerror(errno));
        return 1;
    }

    free(fname);

    if (dns_ip != NULL)
        if (pipe_set_flag(fd, F_DNS_NSIP, dns_nip) < 0)
            fprintf(stderr, "%s: Error setting namserver IP %s, continuing anyway.\n", argv[0], dns_ip);

    size_t len = pipe_get_flag(fd, F_PRESSURE);
    if (!len)
    {
        destroy_pipe(fd, 0);
        fprintf(stderr, "%s: Could not receive IP address.\n", argv[0]);
        return 1;
    }

    char ip[len + 1];
    memset(ip, 0, len + 1);

    stream_recv(fd, ip, len, O_BLOCKING);

    printf("IP address: %s\n", ip);

    destroy_pipe(fd, 0);

    return 0;
}
