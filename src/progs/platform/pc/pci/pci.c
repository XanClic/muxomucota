#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <drivers.h>
#include <drivers/pci.h>
#include <drivers/ports.h>
#include <sys/stat.h>


#define __pci_inX(w, off_mask) \
    static inline uint##w##_t pci_in##w(int bus, int device, int function, int offset) \
    { \
        uint##w##_t ret; \
        out32(0xcf8, 0x80000000 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) | ((function & 0x07) << 8) | (offset & 0xfc)); \
        ret = in##w(0xcfc + (offset & off_mask)); \
        return ret; \
    }

__pci_inX( 8, 3);
__pci_inX(16, 2);
__pci_inX(32, 0);


#define __pci_outX(w, off_mask) \
    static inline void pci_out##w(int bus, int device, int function, int offset, uint##w##_t value) \
    { \
        out32(0xcf8, 0x80000000 | ((bus & 0xff) << 16) | ((device & 0x1f) << 11) | ((function & 0x07) << 8) | (offset & 0xfc)); \
        out##w(0xcfc + (offset & off_mask), value); \
    }

__pci_outX( 8, 3);
__pci_outX(16, 2);
__pci_outX(32, 0);


struct bus
{
    struct bus *sibling;
    struct device *devices;

    int bus;
};

struct device
{
    struct bus *bus;
    struct device *sibling;
    struct function *functions;

    int device;
};

struct function
{
    struct device *device;
    struct function *sibling;

    int function;
};


struct bus *first_bus = NULL;


static void add_function(int bus, int device, int function)
{
    struct bus *pbus;
    for (pbus = first_bus; (pbus != NULL) && (pbus->bus != bus); pbus = pbus->sibling);

    if (pbus == NULL)
    {
        pbus = malloc(sizeof(*pbus));
        pbus->devices = NULL;
        pbus->bus     = bus;

        pbus->sibling = first_bus;
        first_bus = pbus;
    }

    struct device *pdev;
    for (pdev = pbus->devices; (pdev != NULL) && (pdev->device != device); pdev = pdev->sibling);

    if (pdev == NULL)
    {
        pdev = malloc(sizeof(*pdev));
        pdev->bus       = pbus;
        pdev->functions = NULL;
        pdev->device    = device;

        pdev->sibling = pbus->devices;
        pbus->devices = pdev;
    }

    struct function *pfnc;
    for (pfnc = pdev->functions; (pfnc != NULL) && (pfnc->function != function); pfnc = pfnc->sibling);

    if (pfnc == NULL)
    {
        pfnc = malloc(sizeof(*pfnc));
        pfnc->device   = pdev;
        pfnc->function = function;

        pfnc->sibling = pdev->functions;
        pdev->functions = pfnc;
    }
}


static void enumerate_bus(int bus)
{
    for (int dev = 0; dev < 32; dev++)
    {
        for (int fnc = 0; fnc < 8; fnc++)
        {
            if (pci_in16(bus, dev, fnc, 0x00) == 0xffff)
            {
                if (fnc)
                    continue;
                else
                    break;
            }

            switch (pci_in8(bus, dev, fnc, 0x0e) & 0x7f)
            {
                case 0:
                {
                    // Normales PCI-Gerät
                    add_function(bus, dev, fnc);
                    break;
                }

                case 1:
                    // PCI-PCI-Bridge
                    enumerate_bus(pci_in8(bus, dev, fnc, 0x19));
            }

            if (!fnc && !(pci_in8(bus, dev, fnc, 0x0e) & 0x80)) // Multi-function?
                break;
        }
    }
}


struct vfs_file
{
    enum
    {
        TYPE_FUNCTION,
        TYPE_DIR,
    } type;

    off_t ofs;
    size_t size;
    char *data;

    struct function *function;
};


static int hexnumlen(int num)
{
    int len = 1;
    while (num >>= 4)
        len++;
    return len;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (flags & O_CREAT)
        return 0;

    char duped[strlen(relpath) + 1];
    strcpy(duped, relpath);

    char *busname = strtok(duped, "/");

    if ((busname == NULL) || !*busname)
    {
        if (flags & O_WRONLY)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;

        f->ofs = 0;
        f->size = 1;

        for (struct bus *bus = first_bus; bus != NULL; bus = bus->sibling)
            f->size += hexnumlen(bus->bus) + 1;

        char *d = f->data = malloc(f->size);

        for (struct bus *bus = first_bus; bus != NULL; bus = bus->sibling)
        {
            sprintf(d, "%x", bus->bus);
            d += strlen(d) + 1;
        }

        *d = 0;

        return (uintptr_t)f;
    }

    char *endptr;
    int busn = strtol(busname, &endptr, 16);

    if (*endptr)
        return 0;

    struct bus *bus;
    for (bus = first_bus; (bus != NULL) && (bus->bus != busn); bus = bus->sibling);

    if (bus == NULL)
        return 0;

    char *devname = strtok(NULL, "/");

    if (devname == NULL)
    {
        if (flags & O_WRONLY)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;

        f->ofs = 0;
        f->size = 1;

        for (struct device *dev = bus->devices; dev != NULL; dev = dev->sibling)
            f->size += hexnumlen(dev->device) + 1;

        char *d = f->data = malloc(f->size);

        for (struct device *dev = bus->devices; dev != NULL; dev = dev->sibling)
        {
            sprintf(d, "%x", dev->device);
            d += strlen(d) + 1;
        }

        *d = 0;

        return (uintptr_t)f;
    }

    int devn = strtol(devname, &endptr, 16);

    if (*endptr)
        return 0;

    struct device *dev;
    for (dev = bus->devices; (dev != NULL) && (dev->device != devn); dev = dev->sibling);

    if (dev == NULL)
        return 0;

    char *fncname = strtok(NULL, "/");

    if (fncname == NULL)
    {
        if (flags & O_WRONLY)
            return 0;

        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_DIR;

        f->ofs = 0;
        f->size = 1;

        for (struct function *fnc = dev->functions; fnc != NULL; fnc = fnc->sibling)
            f->size += hexnumlen(fnc->function) + 1;

        char *d = f->data = malloc(f->size);

        for (struct function *fnc = dev->functions; fnc != NULL; fnc = fnc->sibling)
        {
            sprintf(d, "%x", fnc->function);
            d += strlen(d) + 1;
        }

        *d = 0;

        return (uintptr_t)f;
    }

    int fncn = strtol(fncname, &endptr, 16);

    if (*endptr)
        return 0;

    if (strtok(NULL, "/") != NULL)
        return 0;

    struct function *fnc;
    for (fnc = dev->functions; (fnc != NULL) && (fnc->function != fncn); fnc = fnc->sibling);

    if (fnc == NULL)
        return 0;

    struct vfs_file *f = malloc(sizeof(*f));
    f->type = TYPE_FUNCTION;
    f->ofs = 0;
    f->size = sizeof(struct pci_config_space);
    f->function = fnc;

    return (uintptr_t)f;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*nf));

    memcpy(nf, f, sizeof(*nf));

    if (nf->type == TYPE_DIR)
    {
        nf->data = malloc(nf->size);
        memcpy(nf->data, f->data, nf->size);
    }

    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);

    free(f);
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        return 0;

    size_t copy_sz = (f->size - (size_t)f->ofs > size) ? size : (f->size - (size_t)f->ofs);

    // TODO: Optimieren? D:
    for (int i = 0; i < (int)copy_sz; i++)
        pci_out8(f->function->device->bus->bus, f->function->device->device, f->function->function, f->ofs + i, ((uint8_t *)data)[i]);

    f->ofs += copy_sz;

    return copy_sz;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    size_t copy_sz = (f->size - (size_t)f->ofs > size) ? size : (f->size - (size_t)f->ofs);

    if (f->type == TYPE_DIR)
        memcpy(data, f->data + f->ofs, copy_sz);
    else
        for (int i = 0; i < (int)copy_sz; i++)
            ((uint8_t *)data)[i] = pci_in8(f->function->device->bus->bus, f->function->device->device, f->function->function, f->ofs + i);

    f->ofs += copy_sz;

    return copy_sz;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return ((interface == I_STATABLE) || (interface == I_FILE));
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return f->size - f->ofs;
        case F_POSITION:
            return f->ofs;
        case F_FILESIZE:
            return f->size;
        case F_READABLE:
            return (f->size - f->ofs > 0);
        case F_WRITABLE:
            return (f->type == TYPE_FUNCTION) && (f->size - f->ofs > 0);
        case F_MODE:
            return (f->type == TYPE_DIR) ? (S_IFDIR | S_IRUSR | S_IXUSR) : (S_IFBLK | S_IRUSR | S_IWUSR);
        case F_INODE:
            return (f->type == TYPE_FUNCTION) ? (f->function->function | (f->function->device->device << 3) | (f->function->device->bus->bus << 8)) : 0;
        case F_NLINK:
            return 1;
        case F_UID:
        case F_GID:
            return 0;
        case F_BLOCK_SIZE:
            return 1;
        case F_BLOCK_COUNT:
            return f->size;
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
            return 0;
    }

    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_POSITION:
            f->ofs = value;
            return 0;
        case F_FLUSH:
            return 0;
    }

    return -EINVAL;
}


int main(void)
{
    iopl(3);

    enumerate_bus(0);

    daemonize("pci", true);
}
