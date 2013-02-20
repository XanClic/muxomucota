#include <drivers.h>
#include <vfs.h>


uintptr_t (*__cdi_create_pipe)(const char *relpath, int flags);
uintptr_t (*__cdi_duplicate_pipe)(uintptr_t id);

void (*__cdi_destroy_pipe)(uintptr_t id, int flags);


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
