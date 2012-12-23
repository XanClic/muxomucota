#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <vfs.h>


extern lock_t _pipe_allocation_lock;


int duplicate_pipe(int pipe, int dest)
{
    if (!_pipes[pipe].pid)
    {
        errno = EBADF;
        return -1;
    }


    for (int i = dest; i < __MFILE; i++)
    {
        if (!_pipes[i].pid)
        {
            lock(&_pipe_allocation_lock);

            if (_pipes[i].pid)
                continue;

            _pipes[i].pid = _pipes[pipe].pid;

            unlock(&_pipe_allocation_lock);


            struct ipc_duplicate_pipe ipc_dp = { .id = _pipes[pipe].id };

            _pipes[i].id = ipc_message_synced(_pipes[i].pid, DUPLICATE_PIPE, &ipc_dp, sizeof(ipc_dp));


            if (!_pipes[i].id)
            {
                lock(&_pipe_allocation_lock);
                _pipes[i].pid = 0;
                unlock(&_pipe_allocation_lock);

                errno = ENOENT; // FIXME
                return -1;
            }

            return i;
        }
    }


    errno = EMFILE;
    return -1;
}
