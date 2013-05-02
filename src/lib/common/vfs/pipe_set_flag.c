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
#include <vfs.h>


int pipe_set_flag(int pipe, int flag, uintmax_t value)
{
    if ((pipe < 0) || (pipe >= __MFILE) || !_pipes[pipe].pid)
    {
        errno = EBADF;
        return -1;
    }


    struct ipc_pipe_set_flag ipc_psf = {
        .id = _pipes[pipe].id,
        .flag = flag,
        .value = value
    };

    return (int)ipc_message_synced(_pipes[pipe].pid, PIPE_SET_FLAG, &ipc_psf, sizeof(ipc_psf));
}
