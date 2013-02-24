#include <stdint.h>
#include <stdio.h>
#include <vfs.h>


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: eth-test <eth-if> (e.g. eth-test (eth)/eth0)\n");
        return 1;
    }


    int fd = create_pipe(argv[1], O_RDWR);

    if (fd < 0)
    {
        perror(argv[1]);
        return 1;
    }


    if (!pipe_implements(fd, I_ETHERNET))
    {
        destroy_pipe(fd, 0);

        fprintf(stderr, "%s: Does not implement the ethernet interface.\n", argv[1]);
        return 1;
    }


    uint64_t my_mac = pipe_get_flag(fd, F_MY_MAC);


    uint8_t arping[] = {
        0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01,
        (my_mac >> 40) & 0xff, (my_mac >> 32) & 0xff, (my_mac >> 24) & 0xff, (my_mac >> 16) & 0xff, (my_mac >> 8) & 0xff, my_mac & 0xff, 0x0a, 0x00,
        0x02, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x0a, 0x00, 0x02, 0x02
    };


    pipe_set_flag(fd, F_ETH_PACKET_TYPE, 0x0806);

    pipe_set_flag(fd, F_DEST_MAC, UINT64_C(0xffffffffffff));


    stream_send(fd, arping, sizeof(arping), O_BLOCKING);


    destroy_pipe(fd, 0);


    return 0;
}
