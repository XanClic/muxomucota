#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <drivers/pci.h>


static void enum_device(const char *busnum, const char *devicenum)
{
    char fname[8 + strlen(busnum) + strlen(devicenum)];
    sprintf(fname, "(pci)/%s/%s", busnum, devicenum);

    int fd = create_pipe(fname, O_RDONLY);

    if (fd < 0)
    {
        fprintf(stderr, "Could not list %s: %s\n", fname, strerror(errno));
        return;
    }

    size_t sz = pipe_get_flag(fd, F_PRESSURE);

    char *functions = malloc(sz);
    stream_recv(fd, functions, sz, O_BLOCKING);

    destroy_pipe(fd, 0);


    for (int i = 0; functions[i]; i += strlen(functions + i) + 1)
    {
        char pciname[strlen(fname) + 1 + strlen(functions + i)];
        sprintf(pciname, "%s/%s", fname, functions + i);

        fd = create_pipe(pciname, O_RDONLY);

        if (fd < 0)
        {
            fprintf(stderr, "Could not open %s: %s\n", pciname, strerror(errno));
            continue;
        }

        struct pci_config_space conf_space;
        stream_recv(fd, &conf_space, sizeof(conf_space), O_BLOCKING);

        int bus = strtol(busnum, NULL, 16);
        int dev = strtol(devicenum, NULL, 16);
        int fnc = strtol(functions + i, NULL, 16);

        printf("%02x:%02x.%x %02x%02x%02x: %04x:%04x\n", bus, dev, fnc, conf_space.baseclass, conf_space.subclass, conf_space.interface, conf_space.vendor_id, conf_space.device_id);
    }
}


static void enum_bus(const char *busnum)
{
    char fname[7 + strlen(busnum)];
    sprintf(fname, "(pci)/%s", busnum);

    int fd = create_pipe(fname, O_RDONLY);

    if (fd < 0)
    {
        fprintf(stderr, "Could not list %s: %s\n", fname, strerror(errno));
        return;
    }

    size_t sz = pipe_get_flag(fd, F_PRESSURE);

    char *devices = malloc(sz);
    stream_recv(fd, devices, sz, O_BLOCKING);

    destroy_pipe(fd, 0);


    for (int i = 0; devices[i]; i += strlen(devices + i) + 1)
        enum_device(busnum, devices + i);
}


int main(void)
{
    int fd = create_pipe("(pci)/", O_RDONLY);

    if (fd < 0)
    {
        perror("Could not open (pci)");
        return 1;
    }

    size_t sz = pipe_get_flag(fd, F_PRESSURE);

    char *buses = malloc(sz);
    stream_recv(fd, buses, sz, O_BLOCKING);

    destroy_pipe(fd, 0);


    for (int i = 0; buses[i]; i += strlen(buses + i) + 1)
        enum_bus(buses + i);


    return 0;
}
