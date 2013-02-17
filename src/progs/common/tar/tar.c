#include <compiler.h>
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


struct tar_header
{
    char filename[100];
    char mode[8];
    char uid[8], gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char link[100];
} cc_packed;


struct tar_node
{
    struct tar_node *sibling, *children;

    char *name;

    void *data;
    size_t sz;

    mode_t mode;

    int uid, gid;
    time_t mtime;
};


static struct mountpoint
{
    struct mountpoint *next;
    char *name;

    struct tar_node *tar;
} *mountpoints = NULL;

static lock_t mp_lock = unlocked;


struct file
{
    enum
    {
        T_MP,
        T_NODE,
        T_ROOT
    } type;

    struct mountpoint *mp;

    void *ptr;
    size_t sz;
    off_t off;

    struct tar_node *node;

    bool data_alloced;
};


static inline int parse_tar_number(char *ptr, int length)
{
    int val = 0;

    for (int i = 0; i < length - 1; i++)
        val = (val << 3) + ptr[i] - '0';

    return val;
}


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (!relpath[0])
    {
        if ((flags & O_WRONLY) || (flags & O_CREAT))
            return 0;

        struct file *f = malloc(sizeof(*f));
        f->type = T_ROOT;
        f->off  = 0;

        f->sz = 1;
        for (struct mountpoint *mp = mountpoints; mp != NULL; mp = mp->next)
            f->sz += strlen(mp->name) + 1;

        char *d = f->ptr = malloc(f->sz);

        for (struct mountpoint *mp = mountpoints; mp != NULL; mp = mp->next)
        {
            strcpy(d, mp->name);
            d += strlen(mp->name) + 1;
        }

        *d = 0;

        f->data_alloced = true;

        return (uintptr_t)f;
    }


    if (strchr(relpath + 1, '/') == NULL)
    {
        struct mountpoint *mp;

        for (mp = mountpoints; (mp != NULL) && strcmp(mp->name, relpath + 1); mp = mp->next);

        if ((mp == NULL) && !(flags & O_CREAT))
            return 0;
        else if ((mp != NULL) && (flags & (O_CREAT | O_WRONLY)))
            return 0;


        struct file *f = malloc(sizeof(*f));
        f->type = T_MP;
        f->off  = 0;


        if (mp == NULL)
        {
            f->sz = 1;
            f->ptr = malloc(1);
            *(char *)f->ptr = 0;


            mp = malloc(sizeof(*mp));
            mp->name = strdup(relpath + 1);
            mp->tar  = NULL;

            lock(&mp_lock);

            mp->next = mountpoints;
            mountpoints = mp;

            unlock(&mp_lock);
        }
        else
        {
            f->sz = 1;
            for (struct tar_node *t = mp->tar; t != NULL; t = t->sibling)
                f->sz += strlen(t->name) + 1;

            char *d = f->ptr = malloc(f->sz);

            for (struct tar_node *t = mp->tar; t != NULL; t = t->sibling)
            {
                strcpy(d, t->name);
                d += strlen(t->name) + 1;
            }

            *d = 0;
        }

        f->mp = mp;
        f->data_alloced = true;

        return (uintptr_t)f;
    }


    if (flags & (O_WRONLY | O_CREAT))
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


    struct tar_node *t = mp->tar, *final = t;

    STRTOK_FOREACH(tarpath, "/", part)
    {
        for (; (t != NULL) && strcmp(t->name, part); t = t->sibling);

        if (t == NULL)
        {
            free(duped);
            return 0;
        }

        final = t;
        t = t->children;
    }


    free(duped);


    struct file *f = malloc(sizeof(*f));
    f->type = T_NODE;
    f->off  = 0;
    f->node = final;

    if (S_ISDIR(final->mode))
    {
        f->sz = 1;
        for (struct tar_node *c = final->children; c != NULL; c = c->sibling)
            f->sz += strlen(c->name) + 1;

        char *d = f->ptr = malloc(f->sz);
        for (struct tar_node *c = final->children; c != NULL; c = c->sibling)
        {
            strcpy(d, c->name);
            d += strlen(c->name) + 1;
        }

        *d = 0;

        f->data_alloced = true;
    }
    else
    {
        f->ptr = final->data;
        f->sz = final->sz;
        f->data_alloced = false;
    }

    return (uintptr_t)f;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)flags;

    struct file *f = (struct file *)id;

    if (f->data_alloced)
        free(f->ptr);

    free(f);
}


static void unload_node(struct tar_node *t)
{
    struct tar_node *next;

    for (struct tar_node *c = t->children; c != NULL; c = next)
    {
        next = c->sibling;
        unload_node(c);
    }

    free(t->data);
    free(t->name);

    free(t);
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


    if (f->mp->tar != NULL)
        unload_node(f->mp->tar);

    f->mp->tar = NULL;


    size_t tarsz = pipe_get_flag(nfd, F_PRESSURE);
    struct tar_header *tar = malloc(tarsz), *header = tar;

    stream_recv(nfd, header, tarsz, 0);

    destroy_pipe(nfd, 0);


    while (((uintptr_t)header - (uintptr_t)tar < tarsz) && header->filename[0])
    {
        struct tar_node **tp = &f->mp->tar, *final = NULL;

        bool isdir = header->filename[strlen(header->filename) - 1] == '/';

        STRTOK_FOREACH(header->filename, "/", part)
        {
            if (!*part)
                continue;

            while ((*tp != NULL) && strcmp((*tp)->name, part))
                tp = &(*tp)->sibling;

            if (*tp == NULL)
            {
                final = malloc(sizeof(*final));

                final->children = NULL;
                final->sibling  = NULL;

                final->name  = strdup(part);

                final->data  = NULL;
                final->sz    = 0;

                final->mode  = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;

                final->uid   = 0;
                final->gid   = 0;
                final->mtime = 0;

                *tp = final;
            }

            tp = &(*tp)->children;
        }

        final->sz    = parse_tar_number(header->size, sizeof(header->size));
        final->mode  = parse_tar_number(header->mode, sizeof(header->mode));
        final->uid   = parse_tar_number(header->uid, sizeof(header->uid));
        final->gid   = parse_tar_number(header->gid, sizeof(header->gid));
        final->mtime = parse_tar_number(header->mtime, sizeof(header->mtime));

        if (!(final->mode & S_IFMT))
            final->mode |= isdir ? S_IFDIR : S_IFREG;

        if (final->sz)
        {
            final->data = malloc(final->sz);
            memcpy(final->data, (void *)((uintptr_t)header + 512), final->sz);
        }

        header = (struct tar_header *)((uintptr_t)header + 512 + ((final->sz + 511) & ~511));
    }


    free(tar);


    return size;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;


    struct file *f = (struct file *)id;


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

    if (interface == I_STATABLE)
        return true;

    if ((f->type == T_MP) && (interface == I_FS))
        return true;
    else if ((f->type == T_NODE) && !S_ISDIR(f->node->mode) && (interface == I_FILE))
        return true;

    return false;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    struct file *f = (struct file *)id, *nf = malloc(sizeof(*nf));

    memcpy(nf, f, sizeof(*f));

    if (nf->type == T_ROOT)
    {
        nf->ptr = malloc(nf->sz);
        memcpy(nf->ptr, f->ptr, nf->sz);
    }

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
        case F_MODE:
            switch (f->type)
            {
                case T_ROOT:
                case T_MP:
                    return S_IFDIR | S_IFODEV | S_IRWXU | S_IRWXG | S_IRWXO;
                case T_NODE:
                    return f->node->mode;
            }
        case F_INODE:
            return 0;
        case F_NLINK:
            return 1;
        case F_UID:
            return ((f->type == T_ROOT) || (f->type == T_MP)) ? 0 : f->node->uid;
        case F_GID:
            return ((f->type == T_ROOT) || (f->type == T_MP)) ? 0 : f->node->gid;
        case F_BLOCK_SIZE:
            return ((f->type == T_NODE) && !S_ISDIR(f->node->mode)) ? 512 : 1;
        case F_BLOCK_COUNT:
            return ((f->type == T_NODE) && !S_ISDIR(f->node->mode)) ? ((f->sz + 511) / 512 + 1) : f->sz;
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
            return ((f->type == T_ROOT) || (f->type == T_MP)) ? 0 : f->node->mtime;
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
        case F_FLUSH:
            return 0;
    }

    return -EINVAL;
}


int main(void)
{
    daemonize("tar");
}
