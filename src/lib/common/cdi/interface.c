#include <drivers.h>
#include <vfs.h>


uintptr_t (*__cdi_create_pipe)(const char *relpath, int flags);
uintptr_t (*__cdi_duplicate_pipe)(uintptr_t id);

void (*__cdi_destroy_pipe)(uintptr_t id, int flags);

big_size_t (*__cdi_stream_recv)(uintptr_t id, void *data, big_size_t size, int flags);
big_size_t (*__cdi_stream_send)(uintptr_t id, const void *data, big_size_t size, int flags);

uintmax_t (*__cdi_pipe_get_flag)(uintptr_t id, int flag);
int (*__cdi_pipe_set_flag)(uintptr_t id, int flag, uintmax_t value);

bool (*__cdi_pipe_implements)(uintptr_t id, int interface);

bool (*__cdi_is_symlink)(const char *relpath, char *linkpath);


uintptr_t service_create_pipe(const char *relpath, int flags)
{
    return __cdi_create_pipe(relpath, flags);
}

uintptr_t service_duplicate_pipe(uintptr_t id)
{
    return __cdi_duplicate_pipe(id);
}


void service_destroy_pipe(uintptr_t id, int flags)
{
    __cdi_destroy_pipe(id, flags);
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    return __cdi_stream_recv(id, data, size, flags);
}

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    return __cdi_stream_send(id, data, size, flags);
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    return __cdi_pipe_get_flag(id, flag);
}

int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    return __cdi_pipe_set_flag(id, flag, value);
}


bool service_pipe_implements(uintptr_t id, int interface)
{
    return __cdi_pipe_implements(id, interface);
}


bool service_is_symlink(const char *relpath, char *linkpath)
{
    return __cdi_is_symlink(relpath, linkpath);
}
