#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_pipe_set_flag(void);

uintmax_t _vfs_pipe_set_flag(void)
{
    struct ipc_pipe_set_flag ipc_psf;

    size_t recv = popup_get_message(&ipc_psf, sizeof(ipc_psf));
    assert(recv == sizeof(ipc_psf));


    return service_pipe_set_flag(ipc_psf.id, ipc_psf.flag, ipc_psf.value);
}


int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value) cc_weak;

int service_pipe_set_flag(uintptr_t id, int flag, uintmax_t value)
{
    (void)id;
    (void)flag;
    (void)value;

    errno = ENOTSUP;

    return 0;
}
