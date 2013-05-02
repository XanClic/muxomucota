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
#include <stdbool.h>
#include <vfs.h>


bool pipe_implements(int pipe, int interface)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return false;
    }


    struct ipc_pipe_implements ipc_pi = {
        .id = _pipes[pipe].id,
        .interface = interface
    };


    return (bool)ipc_message_synced(_pipes[pipe].pid, PIPE_IMPLEMENTS, &ipc_pi, sizeof(ipc_pi));
}
