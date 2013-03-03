#include <drivers.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <vfs.h>
#include <sys/stat.h>


enum file_type
{
    TYPE_NULL = 1,
    TYPE_ZERO
};


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)flags;

    if (!strcmp(relpath, "/null"))
        return TYPE_NULL;
    else if (!strcmp(relpath, "/zero"))
        return TYPE_ZERO;

    errno = ENOENT;
    return 0;
}


uintptr_t service_duplicate_pipe(uintptr_t id)
{
    return id;
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)id;
    (void)flags;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;

    return interface == I_STATABLE;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    switch (flag)
    {
        case F_PRESSURE:
            return (id == TYPE_ZERO) ? 1 : 0;

        case F_READABLE:
            return id == TYPE_ZERO;

        case F_WRITABLE:
            return id == TYPE_NULL;

        case F_INODE:
            return 0;

        case F_MODE:
            return S_IFCHR | 0666;

        case F_NLINK:
        case F_BLOCK_SIZE:
            return 1;

        case F_UID:
        case F_GID:
        case F_ATIME:
        case F_MTIME:
        case F_CTIME:
        case F_BLOCK_COUNT:
            return 0;
    }

    errno = EINVAL;
    return 0;
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)id;
    (void)value;

    switch (flag)
    {
        case F_FLUSH:
            return 0;
    }

    errno = EINVAL;
    return -EINVAL;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;
    (void)data;

    return (id == TYPE_NULL) ? size : 0;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)flags;

    if (id != TYPE_ZERO)
        return 0;

    memset(data, 0, size);

    return size;
}


int main(void)
{
    daemonize("thingamabob", true);
}
