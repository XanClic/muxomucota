#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_create_pipe(void);

uintmax_t _vfs_create_pipe(void)
{
    size_t total_length = popup_get_message(NULL, 0);

    assert(total_length > sizeof(int));

    char in_data[total_length];

    popup_get_message(in_data, total_length);

    struct ipc_create_pipe *ipc_cp = (struct ipc_create_pipe *)in_data;


    return service_create_pipe(ipc_cp->relpath, ipc_cp->flags);
}


uintptr_t service_create_pipe(const char *relpath, int flags) cc_weak;

uintptr_t service_create_pipe(const char *relpath, int flags)
{
    (void)relpath;
    (void)flags;

    errno = ENOTSUP;

    return 0;
}
