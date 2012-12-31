#include <errno.h>
#include <ipc.h>
#include <stdbool.h>
#include <vfs.h>


bool pipe_implements(int pipe, int interface)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return false;
    }


    struct ipc_pipe_implements ipc_pi = {
        .id = _pipes[pipe].id,
        .interface = interface
    };


    return (bool)ipc_message_synced(_pipes[pipe].pid, PIPE_IMPLEMENTS, &ipc_pi, sizeof(ipc_pi));
}
