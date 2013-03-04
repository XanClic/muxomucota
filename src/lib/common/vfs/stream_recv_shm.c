#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <stdint.h>
#include <vfs.h>


big_size_t stream_recv_shm(int pipe, uintptr_t shmid, big_size_t size, int flags)
{
    assert((pipe >= 0) && (pipe < __MFILE));

    if (!_pipes[pipe].pid)
    {
        errno = EBADF;
        return 0;
    }


    struct ipc_stream_recv ipc_sr = {
        .id = _pipes[pipe].id,
        .flags = flags,
        .size = size
    };


    return ipc_shm_message_synced(_pipes[pipe].pid, STREAM_RECV, shmid, &ipc_sr, sizeof(ipc_sr));
}
