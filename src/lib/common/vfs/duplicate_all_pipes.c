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
#include <ipc.h>
#include <lock.h>
#include <vfs.h>


extern lock_t _pipe_allocation_lock;


void duplicate_all_pipes(void)
{
    lock(&_pipe_allocation_lock);


    for (int i = 0; i < __MFILE; i++)
    {
        if (_pipes[i].pid)
        {
            struct ipc_duplicate_pipe ipc_dp = { .id = _pipes[i].id };

            _pipes[i].id = ipc_message_synced(_pipes[i].pid, DUPLICATE_PIPE, &ipc_dp, sizeof(ipc_dp));


            assert(_pipes[i].id);
        }
    }


    unlock(&_pipe_allocation_lock);
}
