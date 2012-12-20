#include <arch-vmem.h>
#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


extern struct pipe _pipes[__MFILE];


big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags)
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
        .offset = 0,
        .size = size
    };


    void *start = NULL;
    uintptr_t align_ofs = 0;

    if (!IS_ALIGNED(data))
    {
        start = aligned_alloc(PAGE_SIZE, PAGE_SIZE);

        align_ofs = (uintptr_t)data % PAGE_SIZE;

        ipc_ss.offset = align_ofs;

        // Passts in eine Page?
        if (size <= PAGE_SIZE - align_ofs)
        {
            memset(start, 0, align_ofs);
            memcpy((void *)((uintptr_t)start + align_ofs), data, size);
            memset((void *)((uintptr_t)start + align_ofs + (uintptr_t)size), 0, PAGE_SIZE - align_ofs - size);

            int pc = 1; // page count

            uintptr_t shm = shm_make(1, &start, &pc);

            assert(shm);

            // FIXME: s. u.
            uintptr_t retval = ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shm, &ipc_ss, sizeof(ipc_ss));

            shm_unmake(shm);

            return retval;
        }

        memset(start, 0, align_ofs);
        memcpy((void *)((uintptr_t)start + align_ofs), data, PAGE_SIZE - align_ofs);
    }


    size_t data_block_size = _alloced_size(data);

    void *end = NULL;

    // TODO: Ist das wirklich in Ordnung? Am Ende des Bereichs können noch
    // andere Daten liegen, dann sollte man wirklich nur size auswerten.
    // Andererseits kann der Nutzer am Anfang extra einen Block so reserviert
    // haben, dass SHM direkt geht, möchte aber kein Vielfaches der Pagegröße
    // senden.
    if (!IS_ALIGNED((uintptr_t)data + data_block_size) && !IS_ALIGNED((uintptr_t)data + size))
    {
        end = aligned_alloc(PAGE_SIZE, PAGE_SIZE);

        uintptr_t final_sz = ((uintptr_t)data + size) % PAGE_SIZE;

        memcpy(end, (void *)((uintptr_t)data + (uintptr_t)size - final_sz), final_sz);
        memset((void *)((uintptr_t)end + final_sz), 0, PAGE_SIZE - final_sz);
    }


    void *val[3]; // vaddr list
    int pcl[3]; // page count list

    val[0] = start;
    pcl[0] = (start != NULL) ? 1 : 0;

    val[1] = (start != NULL) ? (void *)((uintptr_t)data + PAGE_SIZE - align_ofs) : (void *)data;
    pcl[1] = (size + ((start != NULL) ? align_ofs : PAGE_SIZE) - 1) / PAGE_SIZE;

    if (end != NULL)
        pcl[1]--;

    val[2] = end;
    pcl[2] = (end != NULL) ? 1 : 0;


    uintptr_t shm = shm_make(3, val, pcl);

    assert(shm);


    // FIXME: synced? Bah. Zumindest Option für non-synced anbieten. Und das
    // irgendwie bauen.
    // FIXME: uintptr_t < big_size_t
    uintptr_t retval = ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shm, &ipc_ss, sizeof(ipc_ss));


    shm_unmake(shm);


    return retval;
}
