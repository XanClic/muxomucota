/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vmem.h>
#include <vfs.h>
#include <cdi.h>
#include <cdi/lists.h>
#include <cdi/pci.h>
#include <drivers/memory.h>
#include <drivers/pci.h>

#ifdef X86
#include <drivers/ports.h>
#endif


#ifdef X86
void cdi_pci_alloc_ioports(struct cdi_pci_device *device)
{
    (void)device;

    iopl(3);
}

void cdi_pci_free_ioports(struct cdi_pci_device *device)
{
    (void)device;
}
#endif


void cdi_pci_alloc_memory(struct cdi_pci_device *device)
{
    struct cdi_pci_resource *res;

    for (int i = 0; (res = cdi_list_get(device->resources, i)) != NULL; i++)
        if (res->type == CDI_PCI_MEMORY)
            res->address = map_memory(res->start, res->length, VMM_UR | VMM_UW | VMM_CD);
}

void cdi_pci_free_memory(struct cdi_pci_device *device)
{
    struct cdi_pci_resource *res;

    for (int i = 0; (res = cdi_list_get(device->resources, i)) != NULL; i++)
        if (res->type == CDI_PCI_MEMORY)
            unmap_memory(res->address, res->length);
}


#ifdef X86
// FIXME
#define __pci_inX(n, w, off_mask) \
    uint##w##_t cdi_pci_config_read##n(struct cdi_pci_device *device, uint8_t offset) \
    { \
        uint##w##_t ret; \
        out32(0xcf8, 0x80000000 \
                     | ((device->bus & 0xff) << 16) \
                     | ((device->dev & 0x1f) << 11) \
                     | ((device->function & 0x07) << 8) \
                     | (offset & 0xfc)); \
        ret = in##w(0xcfc + (offset & off_mask)); \
        return ret; \
    }

__pci_inX(b,  8, 3)
__pci_inX(w, 16, 2)
__pci_inX(l, 32, 0)


#define __pci_outX(n, w, off_mask) \
    void cdi_pci_config_write##n(struct cdi_pci_device *device, uint8_t offset, uint##w##_t value) \
    { \
        out32(0xcf8, 0x80000000 \
                     | ((device->bus & 0xff) << 16) \
                     | ((device->dev& 0x1f) << 11) \
                     | ((device->function & 0x07) << 8) \
                     | (offset & 0xfc)); \
        out##w(0xcfc + (offset & off_mask), value); \
    }

__pci_outX(b,  8, 3)
__pci_outX(w, 16, 2)
__pci_outX(l, 32, 0)
#endif


static void serve_device(struct cdi_driver *drv, const char *devname, struct pci_config_space *conf_space, int bus, int device, int function, cdi_list_t list)
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

        if (!val)
            continue;

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

                i++;
            }
        }
    }


    cdi_pci_dev->meta = strdup(devname);


    if (list != NULL)
        cdi_list_push(list, cdi_pci_dev);
    else
    {
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
    }
}


static void enum_something(struct cdi_driver *drv, const char *basename, int p1, int p2, int p3, cdi_list_t list)
{
    int fd = create_pipe(basename, O_RDONLY);

    if (fd < 0)
        return;


    if (p1 != -1)
    {
        struct pci_config_space conf_space;
        stream_recv(fd, &conf_space, sizeof(conf_space), O_BLOCKING);

        destroy_pipe(fd, 0);

        serve_device(drv, basename, &conf_space, p1, p2, p3, list);

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

        enum_something(drv, entry_name, p2, p3, strtol(entries + i, NULL, 16), list);
    }
}


void cdi_osdep_pci_collect(struct cdi_driver *drv);

void cdi_osdep_pci_collect(struct cdi_driver *drv)
{
    if (drv->bus != CDI_PCI)
        return;

    enum_something(drv, "(pci)", -1, -1, -1, NULL);
}


void cdi_pci_get_all_devices(cdi_list_t list)
{
    enum_something(NULL, "(pci)", -1, -1, -1, list);
}
