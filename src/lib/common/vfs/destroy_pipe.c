#include <assert.h>
#include <ipc.h>
#include <lock.h>
#include <stdint.h>
#include <sys/types.h>
#include <vfs.h>


extern struct pipe _pipes[__MFILE];

extern lock_t _pipe_allocation_lock;


void destroy_pipe(int pipe, int flags)
{
    assert((pipe >= 0) && (pipe < __MFILE));


    lock(&_pipe_allocation_lock);

    pid_t pid = _pipes[pipe].pid;
    uintptr_t id = _pipes[pipe].id;

    _pipes[pipe].pid = 0;

    unlock(&_pipe_allocation_lock);


    if (pid)
    {
        struct ipc_destroy_pipe ipc_dp = { .id = id, .flags = flags };

        ipc_message(pid, DESTROY_PIPE, &ipc_dp, sizeof(ipc_dp));
    }
}
