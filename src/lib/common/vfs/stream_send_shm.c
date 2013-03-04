#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <stdint.h>
#include <vfs.h>


big_size_t stream_send_shm(int pipe, uintptr_t shmid, big_size_t size, int flags)
{
    assert((pipe >= 0) && (pipe < __MFILE));

    if (!_pipes[pipe].pid)
    {
        errno = EBADF;
        return 0;
    }


    struct ipc_stream_send ipc_ss = {
        .id = _pipes[pipe].id,
        .flags = flags,
        .size = size
    };


    if (flags & O_NONBLOCK)
    {
        ipc_shm_message(_pipes[pipe].pid, STREAM_SEND, shmid, &ipc_ss, sizeof(ipc_ss));
        return size;
    }

    return ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shmid, &ipc_ss, sizeof(ipc_ss));
}
