#include <assert.h>
#include <ipc.h>
#include <lock.h>
#include <vfs.h>


extern lock_t _pipe_allocation_lock;


void duplicate_all_pipes(void)
{
    lock(&_pipe_allocation_lock);


    for (int i = 0; i < __MFILE; i++)
    {
        if (_pipes[i].pid)
        {
            struct ipc_duplicate_pipe ipc_dp = { .id = _pipes[i].id };

            _pipes[i].id = ipc_message_synced(_pipes[i].pid, DUPLICATE_PIPE, &ipc_dp, sizeof(ipc_dp));


            assert(_pipes[i].id);
        }
    }


    unlock(&_pipe_allocation_lock);
}
