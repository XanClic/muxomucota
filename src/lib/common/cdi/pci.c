#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/pci.h>
#include <drivers/pci.h>

#ifdef X86
#include <drivers/ports.h>
#endif


cdi_list_t cdi_osdep_devices = NULL;


#ifdef X86
void cdi_pci_alloc_ioports(struct cdi_pci_device *device)
{
    (void)device;

    iopl(3);
}
#endif


static void serve_device(struct cdi_driver *drv, const char *devname, struct pci_config_space *conf_space, int bus, int device, int function)
{
    if (conf_space->header_type & 0x7f)
        return;

    struct cdi_pci_device *cdi_pci_dev = malloc(sizeof(*cdi_pci_dev));

    cdi_pci_dev->bus_data.bus_type = CDI_PCI;

    cdi_pci_dev->bus = bus;
    cdi_pci_dev->dev = device;
    cdi_pci_dev->function = function;

    cdi_pci_dev->vendor_id = conf_space->vendor_id;
    cdi_pci_dev->device_id = conf_space->device_id;

    cdi_pci_dev->class_id = conf_space->baseclass;
    cdi_pci_dev->subclass_id = conf_space->subclass;
    cdi_pci_dev->interface_id = conf_space->interface;

    cdi_pci_dev->rev_id = conf_space->revision;

    cdi_pci_dev->irq = conf_space->common.interrupt_line;


    int fd = create_pipe(devname, O_RDWR);

    if (fd < 0)
        return;


    cdi_pci_dev->resources = cdi_list_create();

    for (int i = 0; i < 6; i++)
    {
        uint32_t val = conf_space->hdr0.bar[i];

        if (val & 1)
        {
            struct cdi_pci_resource *res = malloc(sizeof(*res));

            res->type    = CDI_PCI_IOPORTS;
            res->start   = val & ~3;
            res->index   = i;
            res->address = NULL;

            pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
            uint32_t mask = 0xfffffffc;
            stream_send(fd, &mask, sizeof(mask), O_BLOCKING);

            pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
            stream_recv(fd, &mask, sizeof(mask), O_BLOCKING);

            res->length = ~(mask & 0xfffffffc) + 0x1;

            pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
            stream_send(fd, &val, sizeof(val), O_BLOCKING);

            cdi_list_push(cdi_pci_dev->resources, res);
        }
        else
        {
            int type = (val >> 1) & 3;

            if ((type == 0) || (type == 1))
            {
                struct cdi_pci_resource *res = malloc(sizeof(*res));

                res->type    = CDI_PCI_MEMORY;
                res->start   = val & ~0xf;
                res->index   = i;
                res->address = NULL;

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                uint32_t mask = 0xfffffff0;
                stream_send(fd, &mask, sizeof(mask), O_BLOCKING);

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                stream_recv(fd, &mask, sizeof(mask), O_BLOCKING);

                res->length = ~(mask & 0xfffffff0) + 0x1;

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                stream_send(fd, &val, sizeof(val), O_BLOCKING);

                cdi_list_push(cdi_pci_dev->resources, res);
            }
            else if ((type == 2) && (i < 5))
            {
                struct cdi_pci_resource *res = malloc(sizeof(*res));

                uint64_t val64 = val | ((uint64_t)conf_space->hdr0.bar[i + 1] << 32);

                res->type    = CDI_PCI_MEMORY;
                res->start   = val64 & ~UINT64_C(0xf);
                res->index   = i;
                res->address = NULL;

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                uint64_t mask = UINT64_C(0xfffffffffffffff0);
                stream_send(fd, &mask, sizeof(mask), O_BLOCKING);

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                stream_recv(fd, &mask, sizeof(mask), O_BLOCKING);

                res->length = ~(mask & UINT64_C(0xfffffffffffffff0)) + 0x1;

                pipe_set_flag(fd, F_POSITION, offsetof(struct pci_config_space, hdr0.bar[i]));
                stream_send(fd, &val64, sizeof(val64), O_BLOCKING);

                cdi_list_push(cdi_pci_dev->resources, res);
            }
        }
    }


    cdi_pci_dev->meta = strdup(devname);


    struct cdi_device *dev = drv->init_device((struct cdi_bus_data *)cdi_pci_dev);

    if (dev == NULL)
    {
        struct cdi_pci_resource *res;
        while ((res = cdi_list_pop(cdi_pci_dev->resources)) != NULL)
            free(res);
        cdi_list_destroy(cdi_pci_dev->resources);

        free(cdi_pci_dev->meta);
        free(cdi_pci_dev);

        return;
    }


    if (cdi_osdep_devices == NULL)
        cdi_osdep_devices = cdi_list_create();

    cdi_list_push(cdi_osdep_devices, dev);
}


static void enum_something(struct cdi_driver *drv, const char *basename, int p1, int p2, int p3)
{
    int fd = create_pipe(basename, O_RDONLY);

    if (fd < 0)
        return;


    if (p1 != -1)
    {
        struct pci_config_space conf_space;
        stream_recv(fd, &conf_space, sizeof(conf_space), O_BLOCKING);

        destroy_pipe(fd, 0);

        serve_device(drv, basename, &conf_space, p1, p2, p3);

        return;
    }


    size_t sz = pipe_get_flag(fd, F_PRESSURE);

    char entries[sz];
    stream_recv(fd, entries, sz, O_BLOCKING);

    destroy_pipe(fd, 0);


    for (int i = 0; entries[i]; i += strlen(entries + i) + 1)
    {
        char entry_name[strlen(basename) + 1 + strlen(entries + i) + 1];
        sprintf(entry_name, "%s/%s", basename, entries + i);

        enum_something(drv, entry_name, p2, p3, strtol(entries + i, NULL, 16));
    }
}


void cdi_osdep_pci_collect(struct cdi_driver *drv);

void cdi_osdep_pci_collect(struct cdi_driver *drv)
{
    if (drv->bus != CDI_PCI)
        return;

    enum_something(drv, "(pci)", -1, -1,-1);
}
