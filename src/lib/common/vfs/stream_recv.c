#include <arch-vmem.h>
#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


big_size_t stream_recv(int pipe, void *data, big_size_t size, int flags)
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


    // TODO: Irgendwie optimieren durch Zusammenfassen aufeinanderfolgender
    // Pages und so. Aber so ists ja nicht fatal.


    uintptr_t start = (uintptr_t)data & ~(PAGE_SIZE - 1);
    uintptr_t end   = ((uintptr_t)data + (ptrdiff_t)size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);


    int page_count = (end - start) / PAGE_SIZE;

    void *val[page_count]; // virtual address list
    int pcl[page_count]; // page count list


    void *alloced[2];
    void *alloced_dst[2];
    size_t alloced_size[2];
    int ai = 0; // alloced index


    ptrdiff_t ofs = (uintptr_t)data % PAGE_SIZE;

    for (int i = 0; i < page_count; i++)
    {
        pcl[i] = 1;

        // empty at end (of this page)
        ptrdiff_t eae = (i < page_count - 1) ? 0 : (end - (uintptr_t)data - (ptrdiff_t)size);
        // bytes to copy now
        ptrdiff_t bcn = PAGE_SIZE - ofs - eae;

        uintptr_t dst = start + (ptrdiff_t)i * PAGE_SIZE;


        if (ofs || eae)
        {
            alloced[ai] = aligned_alloc(PAGE_SIZE, PAGE_SIZE);

            memset(alloced[ai], 0, PAGE_SIZE);

            alloced_dst[ai] = (void *)(dst + ofs);
            alloced_size[ai] = bcn;

            val[i] = alloced[ai++];
        }
        else
            val[i] = (void *)dst;


        ofs = 0;
    }


    uintptr_t shm = shm_make(page_count, val, pcl, (uintptr_t)data % PAGE_SIZE);

    assert(shm);

    // FIXME: uintptr_t < big_size_t
    uintptr_t retval = ipc_shm_message_synced(_pipes[pipe].pid, STREAM_RECV, shm, &ipc_sr, sizeof(ipc_sr));

    shm_unmake(shm);


    for (int i = 0; i < ai; i++)
    {
        memcpy(alloced_dst[i], (uint8_t *)alloced[i] + ((uintptr_t)alloced_dst[i] % PAGE_SIZE), alloced_size[i]);

        free(alloced[i]);
    }


    return retval;
}
