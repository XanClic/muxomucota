#include <compiler.h>
#include <drivers.h>
#include <errno.h>
#include <lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


struct tar
{
    char filename[100];
    uint8_t mode[8];
    uint8_t uid[8], gid[8];
    uint8_t size[12];
    uint8_t checksum[8];
    uint8_t typeflag;
} cc_packed;


static struct mountpoint
{
    struct mountpoint *next;
    char *name;

    struct tar *tar;
    size_t tar_size;
} *mountpoints = NULL;

static lock_t mp_lock = unlocked;


struct file
{
    enum
    {
        T_MP,
        T_FILE,
    } type;

    union
    {
        struct mountpoint *mp;

        struct
        {
            void *ptr;
            size_t sz;
            off_t off;
        };
    };
};


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (strchr(relpath + 1, '/') == NULL)
    {
        if (!(flags & O_CREAT))
            return 0;

        struct mountpoint *mp = malloc(sizeof(*mp));
        mp->name = strdup(relpath + 1);
        mp->tar  = NULL;


        lock(&mp_lock);

        mp->next = mountpoints;
        mountpoints = mp;

        unlock(&mp_lock);


        struct file *f = malloc(sizeof(*f));
        f->type = T_MP;
        f->mp   = mp;

        return (uintptr_t)f;
    }


    if (flags & O_WRONLY)
        return 0;


    char *duped = strdup(relpath + 1);
    char *mp_name = strtok(duped, "/");
    char *tarpath = mp_name + strlen(mp_name) + 1;

    struct mountpoint *mp;

    for (mp = mountpoints; (mp != NULL) && strcmp(mp->name, mp_name); mp = mp->next);

    if (mp == NULL)
    {
        free(duped);
        return 0;
    }


    struct tar *header = mp->tar;
    while (((uintptr_t)header - (uintptr_t)mp->tar < mp->tar_size) && header->filename[0])
    {
        int this_size = 0;
        for (int i = 0; i < 11; i++)
            this_size = (this_size << 3) + header->size[i] - '0';

        // FIXME: 100-Zeichen-Dateinamen
        if (!strcmp(header->filename, tarpath))
        {
            free(duped);

            struct file *f = malloc(sizeof(*f));
            f->type = T_FILE;
            f->ptr  = (void *)((uintptr_t)header + 512);
            f->sz   = this_size;
            f->off  = 0;

            return (uintptr_t)f;
        }

        header = (struct tar *)((uintptr_t)header + 512 + ((this_size + 511) & ~511));
    }


    free(duped);

    return 0;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    free((void *)id);
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    struct file *f = (struct file *)id;

    if ((f->type != T_MP) || (flags != F_MOUNT_FILE))
        return 0;


    const char *name = data;

    if (!size || name[size - 1])
        return 0;


    int nfd = create_pipe(name, O_RDONLY);

    if (nfd < 0)
        return 0;


    free(f->mp->tar);

    f->mp->tar_size = pipe_get_flag(nfd, F_PRESSURE);
    f->mp->tar      = malloc(f->mp->tar_size);

    stream_recv(nfd, f->mp->tar, f->mp->tar_size, 0);


    destroy_pipe(nfd, 0);


    return size;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;


    struct file *f = (struct file *)id;

    if (f->type != T_FILE)
        return 0;


    uintptr_t base = (uintptr_t)f->ptr + f->off;

    size_t copysz = f->sz - f->off;

    if (size < copysz)
        copysz = size;


    memcpy(data, (void *)base, copysz);

    f->off += copysz;


    return copysz;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    struct file *f = (struct file *)id;

    if ((f->type == T_MP) && (interface == I_FS))
        return true;
    else if ((f->type == T_FILE) && (interface == I_FILE))
        return true;

    return false;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct file *f = (struct file *)id, *nf = malloc(sizeof(*nf));

    return (uintptr_t)memcpy(nf, f, sizeof(*f));
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    struct file *f = (struct file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return f->sz - f->off;
        case F_POSITION:
            return f->off;
        case F_FILESIZE:
            return f->sz;
        case F_READABLE:
            return (f->sz - f->off) > 0;
        case F_WRITABLE:
            return false;
        case F_FLUSH:
            return 0;
    }

    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct file *f = (struct file *)id;

    switch (flag)
    {
        case F_POSITION:
            f->off = value;
            return 0;
    }

    return -EINVAL;
}


int main(void)
{
    daemonize("tar");
}
