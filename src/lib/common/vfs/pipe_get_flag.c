#include <errno.h>
#include <ipc.h>
#include <vfs.h>


uintmax_t pipe_get_flag(int pipe, int flag)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return -1;
    }


    struct ipc_pipe_get_flag ipc_pgf = {
        .id = _pipes[pipe].id,
        .flag = flag
    };


    // FIXME: Fehlerhinweise zu bekommen wäre sehr geil
    return ipc_message_synced(_pipes[pipe].pid, PIPE_GET_FLAG, &ipc_pgf, sizeof(ipc_pgf));
}
