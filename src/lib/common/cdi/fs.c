#include <assert.h>
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
#include <cdi/lists.h>


// TODO: Locking


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



static void unload_resource(struct cdi_fs_filesystem *fs, struct cdi_fs_res *node)
{
    assert(node->loaded);
    assert(node->stream_cnt > 0);

    struct cdi_fs_stream str = { .fs = fs, .res = node };


    struct cdi_fs_res *parent = node->parent;


    if (!--node->stream_cnt)
        node->res->unload(&str);


    if (parent != NULL)
        unload_resource(fs, parent);
}


static void use_resource(struct cdi_fs_filesystem *fs, struct cdi_fs_res *node)
{
    struct cdi_fs_stream str = { .fs = fs, .res = node };

    if (!node->loaded)
        node->res->load(&str);

    node->stream_cnt++;


    if (node->parent != NULL)
        use_resource(fs, node->parent);
}


static struct cdi_fs_res *find_resource(struct cdi_fs_filesystem *fs, struct cdi_fs_res *base, char *path)
{
    if (!*path)
        return base;


    struct cdi_fs_res *current = base;

    struct cdi_fs_stream str = { .fs = fs };

    STRTOK_FOREACH(path, "/", pathpart)
    {
        str.res = current;

        if (!current->loaded)
            current->res->load(&str);

        current->stream_cnt++;


        if (current->dir == NULL)
        {
            unload_resource(fs, current);
            return NULL;
        }


        cdi_list_t current_dir = current->dir->list(&str);

        struct cdi_fs_res *child = NULL;

        for (int i = 0; i < (int)cdi_list_size(current_dir); i++)
        {
            child = cdi_list_get(current_dir, i);

            if (!strcmp(child->name, pathpart))
                break;
        }

        if (child == NULL)
        {
            unload_resource(fs, current);
            return NULL;
        }

        current = child;
    }


    return current;
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
        TYPE_CLEAN_MP,
        TYPE_FILE
    } type;

    off_t pos;
    size_t size;

    char *data;
    struct mountpoint *mp;
    struct cdi_fs_stream str;
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

        if (flags & O_JUST_STAT)
        {
            file->size = 0;
            file->data = NULL;
            file->str.res = NULL;

            return (uintptr_t)file;
        }

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

        file->str.res = NULL;

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


    struct cdi_fs_res *res = find_resource(fs, fs->root_res, relative);

    free(duped);


    if (res == NULL)
        return 0;


    struct cdi_fs_stream str = { .fs = fs, .res = res };

    if (!res->loaded)
        res->res->load(&str);

    res->stream_cnt++;


    if (res->dir != NULL)
    {
        if (flags & O_WRONLY)
        {
            unload_resource(fs, res);
            return 0;
        }


        cdi_list_t children = res->dir->list(&str);

        struct vfs_file *file = malloc(sizeof(*file));

        file->type = TYPE_DIR;
        file->pos  = 0;
        file->str  = str;

        if (flags & O_JUST_STAT)
        {
            file->size = 0;
            file->data = NULL;

            return (uintptr_t)file;
        }

        file->size = 1;

        for (int i = 0; i < (int)cdi_list_size(children); i++)
            file->size += strlen(((struct cdi_fs_res *)cdi_list_get(children, i))->name) + 1;

        char *d = file->data = malloc(file->size);

        for (int i = 0; i < (int)cdi_list_size(children); i++)
        {
            struct cdi_fs_res *child = cdi_list_get(children, i);
            strcpy(d, child->name);
            d += strlen(child->name) + 1;
        }

        *d = 0;

        return (uintptr_t)file;
    }

    if (res->file == NULL)
    {
        unload_resource(fs, res);
        return 0;
    }


    struct vfs_file *file = malloc(sizeof(*file));

    file->type = TYPE_FILE;
    file->pos  = 0;
    file->str  = str;


    return (uintptr_t)file;
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

    if ((nf->type != TYPE_CLEAN_MP) && (nf->str.res != NULL))
        use_resource(nf->str.fs, nf->str.res);

    return (uintptr_t)nf;
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

    if ((f->type != TYPE_CLEAN_MP) && (f->str.res != NULL))
        unload_resource(f->str.fs, f->str.res);


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
        if (f->data == NULL)
            return 0;

        size_t copy_sz = (size > (size_t)(f->size - f->pos)) ? (size_t)(f->size - f->pos) : size;

        memcpy(data, (void *)((uintptr_t)f->data + (ptrdiff_t)f->pos), copy_sz);

        f->pos += copy_sz;

        return copy_sz;
    }
    else if (f->type == TYPE_FILE)
    {
        size_t count = f->str.res->file->read(&f->str, f->pos, size, data);

        f->pos += count;

        return count;
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

        if (!newfs->root_res->loaded)
            newfs->root_res->res->load(&(struct cdi_fs_stream){ .fs = newfs, .res = newfs->root_res });

        newfs->root_res->stream_cnt++;


        return size;
    }


    size_t count = f->str.res->file->write(&f->str, f->pos, size, data);

    f->pos += count;

    return count;
}


static uintmax_t cdi_fs_pipe_get_flag(uintptr_t id, int flag)
{
    struct vfs_file *f = (struct vfs_file *)id;

    switch (flag)
    {
        case F_PRESSURE:
            switch (f->type)
            {
                case TYPE_DIR:
                    return f->size - f->pos;
                case TYPE_CLEAN_MP:
                    return 0;
                case TYPE_FILE:
                    return f->str.res->res->meta_read(&f->str, CDI_FS_META_SIZE) - f->pos;
            }
        case F_POSITION:
            return f->pos;
        case F_FILESIZE:
            switch (f->type)
            {
                case TYPE_DIR:
                    return f->size;
                case TYPE_CLEAN_MP:
                    return 0;
                case TYPE_FILE:
                    return f->str.res->res->meta_read(&f->str, CDI_FS_META_SIZE);
            }
        case F_READABLE:
            return cdi_fs_pipe_get_flag(id, F_PRESSURE) > 0;
        case F_WRITABLE:
            return (f->type == TYPE_FILE); // TODO
        case F_INODE:
            return 0; // TODO
        case F_MODE:
            if (f->type == TYPE_CLEAN_MP)
                return S_IFDIR | S_IFODEV | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
            else if ((f->type == TYPE_DIR) && (f->str.res == NULL))
                return S_IFDIR | S_IFODEV | S_IRWXU | S_IRWXG | S_IRWXO;
            else
            {
                mode_t mode = (f->type == TYPE_DIR) ? S_IFDIR : S_IFREG;

                // ja was
                if (f->type == S_IFDIR)
                    mode |= (f->str.res->flags.browse ? S_IROTH : 0) | (f->str.res->flags.create_child ? S_IWOTH : 0) | (f->str.res->flags.browse ? S_IXOTH : 0);
                else
                    mode |= (f->str.res->flags.read ? S_IROTH : 0) | (f->str.res->flags.write ? S_IWOTH : 0) | (f->str.res->flags.execute ? S_IXOTH : 0);

                if (f->str.res->acl == NULL)
                    return mode | ((mode & S_IRWXO) << 3) | ((mode & S_IRWXO) << 6);

                bool got_usr = false, got_grp = false;

                for (int i = 0; i < (int)cdi_list_size(f->str.res->acl); i++)
                {
                    struct cdi_fs_acl_entry *acl = cdi_list_get(f->str.res->acl, i);

                    // FIXME
                    if (!got_usr && (acl->type == CDI_FS_ACL_USER_NUMERIC))
                    {
                        got_usr = true;
                        if (f->type == S_IFDIR)
                            mode |= (acl->flags.browse ? S_IRUSR : 0) | (acl->flags.create_child ? S_IWUSR : 0) | (acl->flags.browse ? S_IXUSR : 0);
                        else
                            mode |= (acl->flags.read ? S_IRUSR : 0) | (acl->flags.write ? S_IWUSR : 0) | (acl->flags.execute ? S_IXUSR : 0);
                    }

                    if (!got_grp && (acl->type == CDI_FS_ACL_GROUP_NUMERIC))
                    {
                        got_grp = true;
                        if (f->type == S_IFDIR)
                            mode |= (acl->flags.browse ? S_IRGRP : 0) | (acl->flags.create_child ? S_IWGRP : 0) | (acl->flags.browse ? S_IXGRP : 0);
                        else
                            mode |= (acl->flags.read ? S_IRGRP : 0) | (acl->flags.write ? S_IWGRP : 0) | (acl->flags.execute ? S_IXGRP : 0);
                    }
                }

                return mode;
            }
        case F_NLINK:
            return 1;
        case F_UID:
        case F_GID:
            if ((f->type == TYPE_CLEAN_MP) || (f->str.res == NULL) || (f->str.res->acl == NULL))
                return 0;

            for (int i = 0; i < (int)cdi_list_size(f->str.res->acl); i++)
            {
                struct cdi_fs_acl_entry *acl = cdi_list_get(f->str.res->acl, i);

                if ((flag == F_UID) && (acl->type == CDI_FS_ACL_USER_NUMERIC))
                    return ((struct cdi_fs_acl_entry_usr_num *)acl)->user_id;

                if ((flag == F_GID) && (acl->type == CDI_FS_ACL_GROUP_NUMERIC))
                    return ((struct cdi_fs_acl_entry_grp_num *)acl)->group_id;
            }

            return 0;
        case F_BLOCK_SIZE:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return 1;
            else
                return f->str.res->res->meta_read(&f->str, CDI_FS_META_BLOCKSZ);
        case F_BLOCK_COUNT:
            if (f->type == TYPE_CLEAN_MP)
                return 0;
            else if ((f->type == TYPE_DIR) && (f->str.res == NULL))
                return f->size;
            else
                return f->str.res->res->meta_read(&f->str, CDI_FS_META_USEDBLOCKS);
        case F_ATIME:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return 0;
            else
                return f->str.res->res->meta_read(&f->str, CDI_FS_META_ACCESSTIME);
        case F_MTIME:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return 0;
            else
                return f->str.res->res->meta_read(&f->str, CDI_FS_META_CHANGETIME);
        case F_CTIME:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return 0;
            else
                return f->str.res->res->meta_read(&f->str, CDI_FS_META_CREATETIME);
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
        case F_ATIME:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return -EPERM;
            else
                return f->str.res->res->meta_write(&f->str, CDI_FS_META_ACCESSTIME, value);
        case F_MTIME:
            if ((f->type == TYPE_CLEAN_MP) || ((f->type == TYPE_DIR) && (f->str.res == NULL)))
                return -EPERM;
            else
                return f->str.res->res->meta_write(&f->str, CDI_FS_META_CHANGETIME, value);
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
