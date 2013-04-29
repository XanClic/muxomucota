#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stddef.h>
#include <stdlib.h>
#include <vfs.h>

#include <unistd.h>


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


uintmax_t _vfs_stream_send_msg(void);

uintmax_t _vfs_stream_send_msg(void)
{
    struct ipc_stream_send *ipc_ss;

    size_t recv = popup_get_message(NULL, 0);
    assert(recv >= sizeof(*ipc_ss));

    ipc_ss = malloc(recv);
    popup_get_message(ipc_ss, recv);

    assert(ipc_ss->size == recv - sizeof(*ipc_ss));

    uintmax_t ssz = service_stream_send(ipc_ss->id, ipc_ss + 1, ipc_ss->size, ipc_ss->flags);

    free(ipc_ss);


    return ssz;
}


big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags) cc_weak;

big_size_t service_stream_send(uintptr_t id, const void *data, big_size_t size, int flags)
{
    (void)id;
    (void)data;
    (void)size;
    (void)flags;

    errno = ENOTSUP;

    return 0;
}
