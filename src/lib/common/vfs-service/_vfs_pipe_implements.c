#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <stdbool.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_pipe_implements(void);

uintmax_t _vfs_pipe_implements(void)
{
    struct ipc_pipe_implements ipc_pi;

    size_t recv = popup_get_message(&ipc_pi, sizeof(ipc_pi));
    assert(recv == sizeof(ipc_pi));


    return service_pipe_implements(ipc_pi.id, ipc_pi.interface);
}


bool service_pipe_implements(uintptr_t id, int interface) cc_weak;

bool service_pipe_implements(uintptr_t id, int interface)
{
    (void)id;
    (void)interface;

    errno = ENOTSUP;

    return false;
}
