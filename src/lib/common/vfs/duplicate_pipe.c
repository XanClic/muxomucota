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
#include <vfs.h>


extern lock_t _pipe_allocation_lock;


int duplicate_pipe(int pipe, int dest)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return -1;
    }


    for (int i = dest; i < __MFILE; i++)
    {
        if (!_pipes[i].pid)
        {
            lock(&_pipe_allocation_lock);

            if (_pipes[i].pid)
                continue;

            _pipes[i].pid = _pipes[pipe].pid;

            unlock(&_pipe_allocation_lock);


            struct ipc_duplicate_pipe ipc_dp = { .id = _pipes[pipe].id };

            _pipes[i].id = ipc_message_synced(_pipes[i].pid, DUPLICATE_PIPE, &ipc_dp, sizeof(ipc_dp));


            if (!_pipes[i].id)
            {
                lock(&_pipe_allocation_lock);
                _pipes[i].pid = 0;
                unlock(&_pipe_allocation_lock);

                return -1;
            }

            return i;
        }
    }


    errno = EMFILE;
    return -1;
}
