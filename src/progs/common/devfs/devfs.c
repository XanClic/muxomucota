#include <drivers.h>
#include <errno.h>
#include <lock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <sys/stat.h>
#include <sys/types.h>


static struct device
{
    struct device *next;
    char *name, *dest;
} *devices = NULL;

static rw_lock_t device_lock = RW_LOCK_INITIALIZER;


struct vfs_file
{
    enum
    {
        TYPE_ROOT,
        TYPE_NEW
    } type;

    union
    {
        struct
        {
            off_t ofs;
            size_t size;
            char *data;
        };

        char *name;
    };
};


bool service_is_symlink(const char *relpath, char *linkpath)
{
    if (!*relpath)
        return false;


    rwl_lock_r(&device_lock);

    for (struct device *dev = devices; dev != NULL; dev = dev->next)
    {
        if (!strcmp(dev->name, relpath + 1))
        {
            strcpy(linkpath, dev->dest);

            rwl_unlock_r(&device_lock);
            return true;
        }
    }

    rwl_unlock_r(&device_lock);
    return false;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (!*relpath)
    {
        if (flags & O_WRONLY)
        {
            errno = EACCES;
            return 0;
        }


        struct vfs_file *f = malloc(sizeof(*f));
        f->type = TYPE_ROOT;
        f->ofs = 0;

        f->size = 1;

        rwl_lock_r(&device_lock);

        for (struct device *dev = devices; dev != NULL; dev = dev->next)
            f->size += strlen(dev->name) + 1;

        char *d = f->data = malloc(f->size);

        for (struct device *dev = devices; dev != NULL; dev = dev->next)
        {
            strcpy(d, dev->name);
            d += strlen(dev->name) + 1;
        }

        rwl_unlock_r(&device_lock);

        *d = 0;


        return (uintptr_t)f;
    }


    if (!(flags & O_CREAT))
    {
        errno = ENOENT;
        return 0;
    }

    if (flags & (O_RDONLY | O_WRONLY))
    {
        errno = EACCES;
        return 0;
    }


    rwl_lock_r(&device_lock);

    for (struct device *dev = devices; dev != NULL; dev = dev->next)
    {
        if (!strcmp(dev->name, relpath + 1))
        {
            rwl_unlock_r(&device_lock);

            errno = ENXIO;
            return 0;
        }
    }

    rwl_unlock_r(&device_lock);


    struct vfs_file *f = malloc(sizeof(*f));
    f->type = TYPE_NEW;
    f->name = strdup(relpath + 1);


    return (uintptr_t)f;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*nf));

    memcpy(nf, f, sizeof(*nf));

    switch (nf->type)
    {
        case TYPE_NEW:
            nf->name = strdup(f->name);
            break;

        case TYPE_ROOT:
            nf->data = malloc(nf->size);
            memcpy(nf->data, f->data, nf->size);
            break;
    }

    return (uintptr_t)nf;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_NEW:
            free(f->name);
            break;

        case TYPE_ROOT:
            free(f->data);
            break;
    }

    free(f);
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (f->type)
    {
        case TYPE_NEW:
            return interface == I_FS;

        case TYPE_ROOT:
            return (interface == I_FILE) || (interface == I_STATABLE);
    }

    return false;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    struct vfs_file *f = (struct vfs_file *)id;

    if ((f->type != TYPE_NEW) || !(flags & F_MOUNT_FILE))
    {
        errno = EACCES;
        return 0;
    }


    const char *dest = data;

    if (!size || dest[size - 1])
    {
        errno = EINVAL;
        return 0;
    }


    rwl_lock_r(&device_lock);

    for (struct device *dev = devices; dev != NULL; dev = dev->next)
    {
        if (!strcmp(dev->name, f->name))
        {
            rwl_require_w(&device_lock);

            free(dev->dest);
            dev->dest = strdup(dest);

            rwl_unlock_w(&device_lock);

            return size;
        }
    }

    rwl_unlock_r(&device_lock);


    struct device *dev = malloc(sizeof(*dev));
    dev->name = strdup(f->name);
    dev->dest = strdup(dest);

    rwl_lock_w(&device_lock);

    dev->next = devices;
    devices = dev;

    rwl_unlock_w(&device_lock);


    return size;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type != TYPE_ROOT)
    {
        errno = EACCES;
        return 0;
    }


    size_t copy_sz = (size > f->size - (size_t)f->ofs) ? (f->size - (size_t)f->ofs) : size;

    memcpy(data, f->data + f->ofs, copy_sz);

    f->ofs += copy_sz;


    return copy_sz;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return (f->type == TYPE_ROOT) ? (f->size - f->ofs) : 0;

        case F_READABLE:
            return (f->type == TYPE_ROOT) ? (f->size > f->ofs) : false;

        case F_WRITABLE:
            return f->type == TYPE_NEW;

        case F_FILESIZE:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return f->size;

        case F_POSITION:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return f->ofs;

        case F_INODE:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return 0;

        case F_MODE:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return S_IFDIR | S_IFODEV | 0777;

        case F_NLINK:
        case F_BLOCK_SIZE:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return 1;

        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
        case F_BLOCK_COUNT:
            if (f->type != TYPE_ROOT)
                goto invalid;

            return 0;
    }


invalid:
    errno = EINVAL;
    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_FLUSH:
            return 0;

        case F_POSITION:
            if (f->type != TYPE_ROOT)
                goto invalid;

            f->ofs = (value > f->size) ? f->size : value;
            return 0;
    }


invalid:
    errno = EINVAL;
    return -EINVAL;
}


int main(void)
{
    daemonize("devfs", true);
}
