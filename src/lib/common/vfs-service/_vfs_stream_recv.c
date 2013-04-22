#include <assert.h>
#include <compiler.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_stream_recv(uintptr_t shmid);

uintmax_t _vfs_stream_recv(uintptr_t shmid)
{
    struct ipc_stream_recv ipc_sr;

    size_t recv = popup_get_message(&ipc_sr, sizeof(ipc_sr));
    assert(recv == sizeof(ipc_sr));


    void *dst = shm_open(shmid, VMM_UW);

    uintmax_t rsz = service_stream_recv(ipc_sr.id, dst, ipc_sr.size, ipc_sr.flags);

    shm_close(shmid, dst, true);


    return rsz;
}


uintmax_t _vfs_stream_recv_msg(void);

uintmax_t _vfs_stream_recv_msg(void)
{
    struct ipc_stream_recv ipc_sr;

    size_t recv = popup_get_message(&ipc_sr, sizeof(ipc_sr));
    assert(recv == sizeof(ipc_sr));


    void *dst = malloc(ipc_sr.size);

    uintmax_t rsz = service_stream_recv(ipc_sr.id, dst, ipc_sr.size, ipc_sr.flags);
    popup_set_answer(dst, rsz);

    free(dst);


    return rsz;
}


big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags) cc_weak;

big_size_t service_stream_recv(uintptr_t id, void *data, big_size_t size, int flags)
{
    (void)id;
    (void)data;
    (void)size;
    (void)flags;

    errno = ENOTSUP;

    return 0;
}
