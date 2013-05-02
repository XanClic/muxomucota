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
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_duplicate_pipe(void);

uintmax_t _vfs_duplicate_pipe(void)
{
    struct ipc_duplicate_pipe ipc_dp;

    size_t recv = popup_get_message(&ipc_dp, sizeof(ipc_dp));
    assert(recv == sizeof(ipc_dp));


    return service_duplicate_pipe(ipc_dp.id);
}


uintptr_t service_duplicate_pipe(uintptr_t id) cc_weak;

uintptr_t service_duplicate_pipe(uintptr_t id)
{
    (void)id;

    errno = ENOTSUP;

    return 0;
}
