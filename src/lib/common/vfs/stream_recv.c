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
#include <unistd.h>
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


    // TODO: Das ist etwas willkürlich; außerdem kann man das schon per SHM
    // machen, wenn die Daten bereits perfekt reinkommen (was aber selten genug
    // vorkommen sollte)
    if (size < 16384)
    {
        uintptr_t aid;

        uintmax_t retval = ipc_message_request(_pipes[pipe].pid, STREAM_RECV_MSG, &ipc_sr, sizeof(ipc_sr), &aid);

        if (!aid)
            assert(!retval);
        else
        {
            size_t recvd = popup_get_answer(aid, data, retval);
            assert(retval == recvd);
        }

        return retval;
    }


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

    uintmax_t retval = ipc_shm_message_synced(_pipes[pipe].pid, STREAM_RECV, shm, &ipc_sr, sizeof(ipc_sr));

    shm_unmake(shm, true);


    for (int i = 0; i < ai; i++)
    {
        memcpy(alloced_dst[i], (uint8_t *)alloced[i] + ((uintptr_t)alloced_dst[i] % PAGE_SIZE), alloced_size[i]);

        free(alloced[i]);
    }


    return retval;
}
