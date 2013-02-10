#include <drivers.h>
#include <lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


static struct mountpoint
{
    struct mountpoint *next;
    char *name;
    int fd;
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
        mp->fd   = -1;


        lock(&mp_lock);

        mp->next = mountpoints;
        mountpoints = mp;

        unlock(&mp_lock);


        struct file *f = malloc(sizeof(*f));
        f->type = T_MP;
        f->mp   = mp;

        return (uintptr_t)f;
    }


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


    int nfd = create_pipe(name, O_RDWR);

    if (nfd < 0)
        return 0;


    if (f->mp->fd)
        destroy_pipe(f->mp->fd, 0);

    f->mp->fd = nfd;


    return size;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    struct file *f = (struct file *)id;

    if ((f->type == T_MP) && (interface == I_FS))
        return true;

    return false;
}


int main(void)
{
    daemonize("tar");
}
