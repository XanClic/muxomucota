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


uintmax_t _vfs_pipe_get_flag(void);

uintmax_t _vfs_pipe_get_flag(void)
{
    struct ipc_pipe_get_flag ipc_pgf;

    size_t recv = popup_get_message(&ipc_pgf, sizeof(ipc_pgf));
    assert(recv == sizeof(ipc_pgf));


    return service_pipe_get_flag(ipc_pgf.id, ipc_pgf.flag);
}


uintmax_t service_pipe_get_flag(uintptr_t id, int flag) cc_weak;

uintmax_t service_pipe_get_flag(uintptr_t id, int flag)
{
    (void)id;
    (void)flag;

    errno = ENOTSUP;

    return 0;
}
