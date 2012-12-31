#include <assert.h>
#include <compiler.h>
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_duplicate_pipe(void);

uintmax_t _vfs_duplicate_pipe(void)
{
    struct ipc_duplicate_pipe ipc_dp;

    size_t recv = popup_get_message(&ipc_dp, sizeof(ipc_dp));
    assert(recv == sizeof(ipc_dp));


    return service_duplicate_pipe(ipc_dp.id);
}


uintptr_t service_duplicate_pipe(uintptr_t id) cc_weak;

uintptr_t service_duplicate_pipe(uintptr_t id)
{
    (void)id;

    return 0;
}
