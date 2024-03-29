/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <arch-vmem.h>
#include <assert.h>
#include <errno.h>
#include <ipc.h>
#include <shm.h>
#include <stdlib.h>
#include <string.h>
#include <vfs.h>


big_size_t stream_send(int pipe, const void *data, big_size_t size, int flags)
{
    assert((pipe >= 0) && (pipe < __MFILE));

    if (!_pipes[pipe].pid)
    {
        errno = EBADF;
        return 0;
    }


    // TODO: Das ist schon ein bisschen willkürlich (das einzige nicht
    // willkürliche daran ist, dass die Zahl kleiner als in stream_recv ist,
    // da hier noch ein zusätzliches memcpy drin ist).
    if (size < 12288)
    {
        // TODO: Dass man die Daten hier nochmal kopieren muss, ist doch doof.
        struct ipc_stream_send *ipc_ss = malloc(sizeof(*ipc_ss) + size);
        ipc_ss->id = _pipes[pipe].id;
        ipc_ss->flags = flags;
        ipc_ss->size = size;

        memcpy(ipc_ss + 1, data, size);

        uintmax_t retval;
        if (!(flags & O_NONBLOCK))
            retval = ipc_message_synced(_pipes[pipe].pid, STREAM_SEND_MSG, ipc_ss, sizeof(*ipc_ss) + size);
        else
        {
            // Issue 25
            ipc_message_synced(_pipes[pipe].pid, STREAM_SEND_MSG, ipc_ss, sizeof(*ipc_ss) + size);
            retval = size;
        }

        free(ipc_ss);

        return retval;
    }


    struct ipc_stream_send ipc_ss = {
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
    int ai = 0; // alloced index


    ptrdiff_t ofs = (uintptr_t)data % PAGE_SIZE;

    for (int i = 0; i < page_count; i++)
    {
        pcl[i] = 1;

        // empty at end (of this page)
        ptrdiff_t eae = (i < page_count - 1) ? 0 : (end - (uintptr_t)data - (ptrdiff_t)size);
        // bytes to copy now
        ptrdiff_t bcn = PAGE_SIZE - ofs - eae;

        uintptr_t src = start + (ptrdiff_t)i * PAGE_SIZE;


        if (ofs || eae)
        {
            alloced[ai] = aligned_alloc(PAGE_SIZE, PAGE_SIZE);

            memset(alloced[ai], 0, ofs);
            memcpy((uint8_t *)alloced[ai] + ofs, (uint8_t *)src + ofs, bcn);
            memset((uint8_t *)alloced[ai] + ofs + bcn, 0, eae);

            val[i] = alloced[ai++];
        }
        else
            val[i] = (void *)src;


        ofs = 0;
    }


    uintptr_t shm = shm_make(page_count, val, pcl, (uintptr_t)data % PAGE_SIZE);
    assert(shm);

    uintmax_t retval;

    if (flags & O_NONBLOCK)
    {
        shm_unmake(shm, false);
        // Issue 25
        ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shm, &ipc_ss, sizeof(ipc_ss));
        retval = size;
    }
    else
    {
        retval = ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shm, &ipc_ss, sizeof(ipc_ss));
        shm_unmake(shm, true);
    }


    for (int i = 0; i < ai; i++)
        free(alloced[i]);


    return retval;
}
