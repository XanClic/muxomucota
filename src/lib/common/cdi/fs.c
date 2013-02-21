#include <errno.h>
#include <lock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>
#include <sys/stat.h>
#include <cdi.h>
#include <cdi/fs.h>


static struct cdi_fs_driver *fs_driver;


size_t cdi_fs_data_read(struct cdi_fs_filesystem *fs, uint64_t start, size_t size, void *buffer)
{
    pipe_set_flag(fs->osdep, F_POSITION, start);

    return stream_recv(fs->osdep, buffer, size, O_BLOCKING);
}


size_t cdi_fs_data_write(struct cdi_fs_filesystem *fs, uint64_t start, size_t size, const void *buffer)
{
    pipe_set_flag(fs->osdep, F_POSITION, start);

    return stream_send(fs->osdep, buffer, size, O_BLOCKING);
}


void cdi_fs_driver_init(struct cdi_fs_driver *driver)
{
    driver->drv.type = CDI_FILESYSTEM;
    cdi_driver_init((struct cdi_driver *)driver);
}


void cdi_fs_driver_destroy(struct cdi_fs_driver *driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}



static struct mountpoint
{
    struct mountpoint *next;

    char *name;
    struct cdi_fs_filesystem *fs;
} *mountpoints = NULL;

static lock_t mp_lock = LOCK_INITIALIZER;


struct vfs_file
{
    enum
    {
        TYPE_DIR,
        TYPE_CLEAN_MP
    } type;

    off_t pos;
    size_t size;

    union
    {
        char *data;
        struct mountpoint *mp;
        struct cdi_fs_filesystem *fs;
    };
};


static uintptr_t cdi_fs_create_pipe(const char *path, int flags)
{
    if (!*path)
    {
        if (flags & (O_WRONLY | O_CREAT))
            return 0;


        struct vfs_file *file = malloc(sizeof(*file));

        file->type = TYPE_DIR;
        file->pos  = 0;
        file->size = 1;

        lock(&mp_lock);

        for (struct mountpoint *mp = mountpoints; mp != NULL; mp = mp->next)
            file->size += strlen(mp->name) + 1;

        char *d = file->data = malloc(file->size);

        for (struct mountpoint *mp = mountpoints; mp != NULL; mp = mp->next)
        {
            strcpy(d, mp->name);
            d += strlen(mp->name) + 1;
        }

        unlock(&mp_lock);

        *d = 0;

        return (uintptr_t)file;
    }


    char *duped = strdup(path + 1), *relative;

    char *slash = strchr(duped, '/');

    if (slash == NULL)
        relative = duped + strlen(duped);
    else
    {
        *slash = 0;
        relative = slash + 1;
    }


    lock(&mp_lock);

    struct mountpoint *mp;

    for (mp = mountpoints; (mp != NULL) && strcmp(mp->name, duped); mp = mp->next);

    struct cdi_fs_filesystem *fs = (mp != NULL) ? mp->fs : NULL;

    unlock(&mp_lock);


    if (fs == NULL)
    {
        if (!(flags & O_CREAT))
        {
            free(duped);
            return 0;
        }


        if (mp == NULL)
        {
            mp = malloc(sizeof(*mp));

            mp->fs   = NULL;
            mp->name = strdup(duped);

            lock(&mp_lock);

            mp->next = mountpoints;
            mountpoints = mp;

            unlock(&mp_lock);
        }

        free(duped);

        // FIXME: mp wird hier benutzt, könnte aber währenddessen gelöscht werden oder so
        struct vfs_file *file = malloc(sizeof(*file));

        file->type = TYPE_CLEAN_MP;
        file->mp   = mp;

        return (uintptr_t)file;
    }


    free(duped);


    return 0;
}


static uintptr_t cdi_fs_duplicate_pipe(uintptr_t id)
{
    struct vfs_file *f = (struct vfs_file *)id, *nf = malloc(sizeof(*nf));

    memcpy(nf, f, sizeof(*nf));

    if (nf->type == TYPE_DIR)
    {
        nf->data = malloc(nf->size);
        memcpy(nf->data, f->data, nf->size);
    }

    return 0;
}


static void cdi_fs_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        free(f->data);

    if ((f->type == TYPE_CLEAN_MP) && (f->mp->fs == NULL))
    {
        lock(&mp_lock);

        struct mountpoint **mp;
        for (mp = &mountpoints; (*mp != NULL) && (*mp != f->mp); mp = &(*mp)->next);

        if (*mp == f->mp)
            *mp = f->mp->next;

        free(f->mp->name);
        free(f->mp);

        unlock(&mp_lock);
    }

    free(f);
}


static big_size_t cdi_fs_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->pos > f->size)
        f->pos = f->size;

    if (f->type == TYPE_DIR)
    {
        size_t copy_sz = (size > (size_t)(f->size - f->pos)) ? (size_t)(f->size - f->pos) : size;

        memcpy(data, (void *)((uintptr_t)f->data + (ptrdiff_t)f->pos), copy_sz);

        f->pos += copy_sz;

        return copy_sz;
    }

    return 0;
}


static big_size_t cdi_fs_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    struct vfs_file *f = (struct vfs_file *)id;

    if (f->type == TYPE_DIR)
        return 0;

    if (f->type == TYPE_CLEAN_MP)
    {
        // TODO: Remounten erlauben

        const char *name = data;

        if (!size || name[size - 1])
            return 0;

        bool rdonly = false;

        int base_fd = create_pipe(name, O_RDWR);

        if (base_fd < 0)
        {
            base_fd = create_pipe(name, O_RDONLY);

            if (base_fd < 0)
                return 0;

            rdonly = true;
        }

        struct cdi_fs_filesystem *newfs = malloc(sizeof(*newfs));
        newfs->driver    = fs_driver;
        newfs->error     = CDI_FS_ERROR_NONE;
        newfs->read_only = rdonly;
        newfs->osdep     = base_fd;


        if (!fs_driver->fs_init(newfs))
        {
            free(newfs);
            destroy_pipe(base_fd, 0);

            return 0;
        }


        f->mp->fs = newfs;

        return size;
    }

    return 0;
}


static uintmax_t cdi_fs_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            return f->size - f->pos;
        case F_POSITION:
            return f->pos;
        case F_FILESIZE:
            return f->size;
        case F_INODE:
            return 0; // TODO
        case F_MODE:
            return (f->type == TYPE_DIR) ? (S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO) : (S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO);
        case F_NLINK:
            return 1;
        case F_UID:
            return 0;
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


static int cdi_fs_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_POSITION:
            f->pos = value;
            return 0;
        case F_FLUSH:
            // TODO
            return 0;
    }

    return -EINVAL;
}


static bool cdi_fs_pipe_implements(uintptr_t id, int interface)
{
    struct vfs_file *f = (struct vfs_file *)id;

    if ((interface == I_FILE) || (interface == I_STATABLE))
        return true;

    if ((f->type == TYPE_CLEAN_MP) && (interface == I_FS))
        return true;

    return false;
}


static bool cdi_fs_is_symlink(const char *relpath, char *linkpath)
{
    (void)relpath;
    (void)linkpath;

    // TODO
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


void cdi_fs_driver_register(struct cdi_fs_driver *driver)
{
    __cdi_create_pipe     = cdi_fs_create_pipe;
    __cdi_duplicate_pipe  = cdi_fs_duplicate_pipe;
    __cdi_destroy_pipe    = cdi_fs_destroy_pipe;
    __cdi_stream_recv     = cdi_fs_stream_recv;
    __cdi_stream_send     = cdi_fs_stream_send;
    __cdi_pipe_get_flag   = cdi_fs_pipe_get_flag;
    __cdi_pipe_set_flag   = cdi_fs_pipe_set_flag;
    __cdi_pipe_implements = cdi_fs_pipe_implements;
    __cdi_is_symlink      = cdi_fs_is_symlink;

    driver->drv.init();


    fs_driver = driver;
}
