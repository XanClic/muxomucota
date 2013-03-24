#include <compiler.h>
#include <errno.h>
#include <lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cdi.h>
#include <cdi/storage.h>
#include <sys/stat.h>


struct cdi_storage_partition
{
    uint64_t start_block, block_count;
};

struct bootsec
{
    int8_t some_stuff[0x1BE];
    struct
    {
        uint8_t flags;
        uint8_t first_chs[3];
        uint8_t type;
        uint8_t last_chs[3];
        uint32_t lba, length;
    } cc_packed entry[4];
    uint16_t signature;
} cc_packed;


struct device_list
{
    struct device_list *next;
    struct cdi_storage_device *dev;
};

static struct device_list *devices;
static rw_lock_t devices_lock = RW_LOCK_INITIALIZER;


struct vfs_file
{
    enum
    {
        TYPE_DIR,
        TYPE_DEV
    } type;

    uint64_t pos;

    union
    {
        struct
        {
            char *data;
            size_t size;
        };

        struct
        {
            struct cdi_storage_device *dev;
            struct cdi_storage_partition *part;
        };
    };
};


extern void cdi_osdep_device_found(void);


void cdi_storage_driver_init(struct cdi_storage_driver *driver)
{
    driver->drv.type = CDI_STORAGE;
    cdi_driver_init((struct cdi_driver *)driver);
}


void cdi_storage_device_init(struct cdi_storage_device *device)
{
    cdi_osdep_device_found();


    struct device_list *ndev = malloc(sizeof(*ndev));
    ndev->dev = device;

    rwl_lock_w(&devices_lock);

    ndev->next = devices;
    devices = ndev;

    rwl_unlock_w(&devices_lock);


    device->partition_count = 0;

    if ((device->block_count * device->block_size) < (2 << 20)) // kleiner als 2 MB, eher keine Partitionen
        return;

    struct bootsec *first_sector = malloc((device->block_size < 512) ? 512 : device->block_size);
    ((struct cdi_storage_driver *)device->dev.driver)->read_blocks(device, 0, 1, first_sector);

    if (first_sector->signature != 0xAA55)
    {
        free(first_sector);
        return;
    }

    device->parts = calloc(16, sizeof(device->parts[0])); // 16 sollte doch hoffentlich reichen

    int max_part_count = 4, cont = 1, pi = 0;
    uint_fast32_t extended_pointer = 0;

    while (cont)
    {
        cont = 0;

        for (int i = 0; i < max_part_count; i++)
        {
            if (first_sector->entry[i].lba)
            {
                if ((first_sector->entry[i].type == 0x05) || (first_sector->entry[i].type == 0x0F)) // Extended
                {
                    cont = 1;
                    extended_pointer = first_sector->entry[i].lba;
                    continue;
                }

                device->parts[pi].start_block = first_sector->entry[i].lba;
                device->parts[pi].block_count = first_sector->entry[i].length;

                if (++pi >= 16)
                {
                    cont = 0;
                    break;
                }
            }
        }

        if (cont)
        {
            ((struct cdi_storage_driver *)device->dev.driver)->read_blocks(device, extended_pointer, 1, first_sector);
            max_part_count = 2;
        }
    }

    free(first_sector);
    device->partition_count = pi;
}


void cdi_storage_driver_destroy(struct cdi_storage_driver *driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}


static uintptr_t cdi_storage_create_pipe(const char *relpath, int flags)
{
    if (!*relpath)
    {
        if (flags & O_WRONLY)
        {
            errno = EACCES;
            return 0;
        }


        struct vfs_file *f = malloc(sizeof(*f));

        f->type = TYPE_DIR;
        f->pos = 0;

        f->size = 1;

        rwl_lock_r(&devices_lock);

        for (struct device_list *dev = devices; dev != NULL; dev = dev->next)
        {
            f->size += strlen(dev->dev->dev.name) + 1;

            for (int i = 0; i < dev->dev->partition_count; i++)
                f->size += strlen(dev->dev->dev.name) + 3 + i / 10 + 1;
        }

        char *d = f->data = malloc(f->size);

        for (struct device_list *dev = devices; dev != NULL; dev = dev->next)
        {
            strcpy(d, dev->dev->dev.name);
            d += strlen(dev->dev->dev.name) + 1;

            for (int i = 0; i < dev->dev->partition_count; i++)
            {
                sprintf(d, "%s_p%i", dev->dev->dev.name, i);
                d += strlen(d) + 1;
            }
        }

        rwl_unlock_r(&devices_lock);

        *d = 0;

        return (uintptr_t)f;
    }


    char duped[strlen(relpath)];
    strcpy(duped, relpath + 1);

    char *devname = strtok(duped, "_"), *partname = strtok(NULL, "_");

    if ((partname != NULL) && ((strtok(NULL, "_") != NULL) || (partname[0] != 'p') || !partname[1]))
    {
        errno = ENOENT;
        return 0;
    }


    rwl_lock_r(&devices_lock);

    struct device_list *dev;
    for (dev = devices; dev != NULL; dev = dev->next)
        if (!strcmp(dev->dev->dev.name, devname))
            break;

    rwl_unlock_r(&devices_lock);


    if (dev == NULL)
    {
        errno = ENOENT;
        return 0;
    }


    int pnum = -1;
    if (partname != NULL)
    {
        char *endptr;
        pnum = strtol(partname + 1, &endptr, 10);

        if (*endptr || (pnum < 0) || (pnum >= dev->dev->partition_count))
        {
            errno = ENOENT;
            return 0;
        }
    }


    struct vfs_file *f = malloc(sizeof(*f));

    f->type = TYPE_DEV;
    f->pos = 0;
    f->dev = dev->dev;
    f->part = (pnum < 0) ? NULL : &dev->dev->parts[pnum];


    return (uintptr_t)f;
}


static uintptr_t cdi_storage_duplicate_pipe(uintptr_t id)
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


static void cdi_storage_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);

    free(f);
}


// TODO: Caching

static bool read_blocks(void *dst, struct cdi_storage_device *dev, uint64_t start, uint64_t count)
{
    return ((struct cdi_storage_driver *)dev->dev.driver)->read_blocks(dev, start, count, dst) == 0;
}


static bool write_blocks(const void *src, struct cdi_storage_device *dev, uint64_t start, uint64_t count)
{
    return ((struct cdi_storage_driver *)dev->dev.driver)->write_blocks(dev, start, count, (void *)src) == 0;
}


static void flush(struct cdi_storage_device *dev)
{
    (void)dev;
}


static uintmax_t cdi_storage_pipe_get_flag(uintptr_t id, int flag);

static big_size_t cdi_storage_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
    {
        size_t copy_sz = (size > (size_t)(f->size - f->pos)) ? (size_t)(f->size - f->pos) : size;

        memcpy(data, (void *)((uintptr_t)f->data + (ptrdiff_t)f->pos), copy_sz);

        f->pos += copy_sz;

        return copy_sz;
    }
    else
    {
        big_size_t full_size = cdi_storage_pipe_get_flag(id, F_PRESSURE);
        big_size_t copy_sz = (size > full_size) ? full_size : size;

        if (!copy_sz)
            return 0;

        void *target = data;
        if ((f->pos % f->dev->block_size) || (copy_sz % f->dev->block_size))
            target = malloc(copy_sz + 2 * f->dev->block_size); // FIXME: Optimieren (man muss max. den ersten und letzten Block in so einen Puffer packen)

        bool success = read_blocks(target, f->dev, ((f->part == NULL) ? 0 : f->part->start_block) + f->pos / f->dev->block_size,
                (copy_sz + f->pos % f->dev->block_size + f->dev->block_size - 1) / f->dev->block_size);

        if (success)
        {
            if (target != data)
            {
                memcpy(data, (const char *)target + f->pos % f->dev->block_size, copy_sz);
                free(target);
            }

            f->pos += copy_sz;
        }

        return success ? copy_sz : 0;
    }
}


static big_size_t cdi_storage_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DEV)
    {
        big_size_t full_size = cdi_storage_pipe_get_flag(id, F_PRESSURE);
        big_size_t copy_sz = (size > full_size) ? full_size : size;

        if (!copy_sz)
            return 0;

        const void *target = data;
        if ((f->pos % f->dev->block_size) || (copy_sz % f->dev->block_size))
        {
            char *tmp = malloc(copy_sz + 2 * f->dev->block_size); // FIXME: Optimieren (man muss max. den ersten und letzten Block in so einen Puffer packen)

            if (f->pos % f->dev->block_size)
                read_blocks(tmp, f->dev, ((f->part == NULL) ? 0 : f->part->start_block) + f->pos / f->dev->block_size, 1);

            if (((f->pos + copy_sz) % f->dev->block_size) && (f->pos % f->dev->block_size + copy_sz > f->dev->block_size))
                read_blocks(tmp + ((f->pos % f->dev->block_size + copy_sz) / f->dev->block_size) * f->dev->block_size,
                        f->dev, ((f->part == NULL) ? 0 : f->part->start_block) + (f->pos + copy_sz) / f->dev->block_size, 1);

            memcpy(tmp + f->pos % f->dev->block_size, data, copy_sz);

            target = tmp;
        }

        bool success = write_blocks(target, f->dev, ((f->part == NULL) ? 0 : f->part->start_block) + f->pos / f->dev->block_size,
                (copy_sz + f->pos % f->dev->block_size + f->dev->block_size - 1) / f->dev->block_size);

        if (success)
        {
            if (target != data)
                free((void *)target);

            f->pos += copy_sz;
        }

        return success ? copy_sz : 0;
    }

    errno = EACCES;
    return 0;
}


static uintmax_t cdi_storage_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return cdi_storage_pipe_get_flag(id, F_FILESIZE) - f->pos;

        case F_READABLE:
            return cdi_storage_pipe_get_flag(id, F_FILESIZE) > f->pos;

        case F_WRITABLE:
            return (f->type == TYPE_DEV) && (cdi_storage_pipe_get_flag(id, F_FILESIZE) > f->pos);

        case F_POSITION:
            return f->pos;

        case F_FILESIZE:
            if (f->type == TYPE_DIR)
                return f->size;
            else if (f->part == NULL)
                return f->dev->block_count * f->dev->block_size;
            else
                return f->part->block_count * f->dev->block_size;

        case F_INODE:
        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_CTIME:
        case F_MTIME:
            return 0;

        case F_MODE:
            if (f->type == TYPE_DIR)
                return S_IFDIR | S_IFODEV | S_IRWXU | S_IRWXG | S_IRWXO;
            else
                return S_IFBLK | S_IRWXU | S_IRWXG | S_IRWXO;

        case F_NLINK:
            return 1;

        case F_BLOCK_SIZE:
            if (f->type == TYPE_DIR)
                return 1;
            else
                return f->dev->block_size;

        case F_BLOCK_COUNT:
            if (f->type == TYPE_DIR)
                return f->size;
            else if (f->part == NULL)
                return f->dev->block_count;
            else
                return f->part->block_count;
    }

    errno = EINVAL;
    return 0;
}


static int cdi_storage_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_FLUSH:
            if (f->type == TYPE_DEV)
                flush(f->dev);
            return 0;

        case F_POSITION:
            if (f->type == TYPE_DIR)
            {
                if (value > f->size)
                    value = f->size;
            }
            else
            {
                if (value > f->dev->block_count * f->dev->block_size)
                    value = f->dev->block_count * f->dev->block_size;
            }

            f->pos = value;
            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


static bool cdi_storage_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return (interface == I_FILE) || (interface == I_STATABLE);
}


static bool cdi_storage_is_symlink(const char *relpath, char *linkpath)
{
    (void)relpath;
    (void)linkpath;

    return false;
}


extern uintptr_t (*__cdi_create_pipe)(const char *relpath, int flags);
extern uintptr_t (*__cdi_duplicate_pipe)(uintptr_t id);

extern void (*__cdi_destroy_pipe)(uintptr_t id, int flags);

big_size_t (*__cdi_stream_recv)(uintptr_t id, void *data, big_size_t size, int flags);
big_size_t (*__cdi_stream_send)(uintptr_t id, const void *data, big_size_t size, int flags);

uintmax_t (*__cdi_pipe_get_flag)(uintptr_t id, int flag);
int (*__cdi_pipe_set_flag)(uintptr_t id, int flag, uintmax_t value);

bool (*__cdi_pipe_implements)(uintptr_t id, int interface);

bool (*__cdi_is_symlink)(const char *relpath, char *linkpath);


void cdi_storage_driver_register(struct cdi_storage_driver *driver);


void cdi_storage_driver_register(struct cdi_storage_driver *driver)
{
    __cdi_create_pipe     = cdi_storage_create_pipe;
    __cdi_duplicate_pipe  = cdi_storage_duplicate_pipe;
    __cdi_destroy_pipe    = cdi_storage_destroy_pipe;
    __cdi_stream_recv     = cdi_storage_stream_recv;
    __cdi_stream_send     = cdi_storage_stream_send;
    __cdi_pipe_get_flag   = cdi_storage_pipe_get_flag;
    __cdi_pipe_set_flag   = cdi_storage_pipe_set_flag;
    __cdi_pipe_implements = cdi_storage_pipe_implements;
    __cdi_is_symlink      = cdi_storage_is_symlink;

    driver->drv.init();
}
