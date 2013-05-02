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
        // Issue 25
        ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shmid, &ipc_ss, sizeof(ipc_ss));
        return size;
    }

    return ipc_shm_message_synced(_pipes[pipe].pid, STREAM_SEND, shmid, &ipc_ss, sizeof(ipc_ss));
}
