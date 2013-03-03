#include <errno.h>
#include <ipc.h>
#include <vfs.h>


int pipe_set_flag(int pipe, int flag, uintmax_t value)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return -1;
    }


    struct ipc_pipe_set_flag ipc_psf = {
        .id = _pipes[pipe].id,
        .flag = flag,
        .value = value
    };

    return (int)ipc_message_synced(_pipes[pipe].pid, PIPE_SET_FLAG, &ipc_psf, sizeof(ipc_psf));
}
