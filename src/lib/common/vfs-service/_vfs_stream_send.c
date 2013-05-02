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
