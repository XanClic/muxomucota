#include <assert.h>
#include <compiler.h>
#include <ipc.h>
#include <shm.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_stream_send(uintptr_t shmid);

uintmax_t _vfs_stream_send(uintptr_t shmid)
{
    struct ipc_stream_send ipc_ss;

    size_t recv = popup_get_message(&ipc_ss, sizeof(ipc_ss));
    assert(recv == sizeof(ipc_ss));


    const void *src = shm_open(shmid, VMM_UR);

    uintmax_t ssz = service_stream_send(ipc_ss.id, src, ipc_ss.size, ipc_ss.flags);

    shm_close(shmid, src, true);


    return ssz;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags) cc_weak;

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)id;
    (void)data;
    (void)size;
    (void)flags;

    return 0;
}
