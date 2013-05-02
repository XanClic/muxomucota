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
#include <ipc.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_destroy_pipe(void);

uintmax_t _vfs_destroy_pipe(void)
{
    struct ipc_destroy_pipe ipc_dp;

    size_t recv = popup_get_message(&ipc_dp, sizeof(ipc_dp));
    assert(recv == sizeof(ipc_dp));


    service_destroy_pipe(ipc_dp.id, ipc_dp.flags);

    return 0;
}


void service_destroy_pipe(uintptr_t id, int flags) cc_weak;

void service_destroy_pipe(uintptr_t id, int flags)
{
    (void)id;
    (void)flags;
}
