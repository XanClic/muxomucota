#include <assert.h>
#include <compiler.h>
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_destroy_pipe(void);

uintmax_t _vfs_destroy_pipe(void)
{
    struct ipc_destroy_pipe ipc_dp;

    size_t recv = popup_get_message(&ipc_dp, sizeof(ipc_dp));
    assert(recv == sizeof(ipc_dp));


    service_destroy_pipe(ipc_dp.id, ipc_dp.flags);

    return 0;
}


void service_destroy_pipe(uintptr_t id, int flags) cc_weak;

void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)id;
    (void)flags;
}
