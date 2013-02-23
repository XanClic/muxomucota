#include <drivers.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>


// TODO


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    if (!strcmp(relpath, "/register"))
    {
        if (flags & O_RDONLY)
            return 0;

        return 1;
    }
    else if (!strcmp(relpath, "/receive"))
    {
        if (flags & O_RDONLY)
            return 0;

        return 2;
    }

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


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)flags;

    switch (id)
    {
        case 1:
            if (size < sizeof(uintptr_t))
                return 0;

            printf("(eth) registered %i/%p\n", getppid(), (void *)*(uintptr_t *)data);
            return sizeof(uintptr_t);

        case 2:
            printf("(eth) received packet (%i B) from %i\n", (int)size, getppid());
            return size;
    }

    return 0;
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    return ((id == 2) && (interface == I_DEVICE_CONTACT));
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    switch (flag)
    {
        case F_DEVICE_ID:
            if (id != 2)
                return -EINVAL;

            printf("(eth) receiving device ID is %p\n", (void *)(uintptr_t)value);
            return 0;

        case F_FLUSH:
            return 0;
    }

    return -EINVAL;
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;

    switch (flag)
    {
        case F_PRESSURE:
            return 0;
        case F_READABLE:
            return false;
        case F_WRITABLE:
            return true;
    }

    return 0;
}


int main(void)
{
    daemonize("eth");
}
