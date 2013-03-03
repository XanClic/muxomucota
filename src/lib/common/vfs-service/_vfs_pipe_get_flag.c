#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_pipe_get_flag(void);

uintmax_t _vfs_pipe_get_flag(void)
{
    struct ipc_pipe_get_flag ipc_pgf;

    size_t recv = popup_get_message(&ipc_pgf, sizeof(ipc_pgf));
    assert(recv == sizeof(ipc_pgf));


    return service_pipe_get_flag(ipc_pgf.id, ipc_pgf.flag);
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag) cc_weak;

uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;
    (void)flag;

    errno = ENOTSUP;

    return 0;
}
