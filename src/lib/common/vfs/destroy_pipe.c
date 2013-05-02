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

#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <stdint.h>
#include <sys/types.h>
#include <vfs.h>


extern lock_t _pipe_allocation_lock;


void destroy_pipe(int pipe, int flags)
{
    if ((pipe < 0) || (pipe >= __MFILE))
    {
        errno = EBADF;
        return;
    }


    lock(&_pipe_allocation_lock);

    pid_t pid = _pipes[pipe].pid;
    uintptr_t id = _pipes[pipe].id;

    _pipes[pipe].pid = 0;

    unlock(&_pipe_allocation_lock);


    if (pid)
    {
        struct ipc_destroy_pipe ipc_dp = { .id = id, .flags = flags };

        ipc_message(pid, DESTROY_PIPE, &ipc_dp, sizeof(ipc_dp));
    }
}
