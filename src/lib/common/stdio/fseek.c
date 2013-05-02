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
#include <stdio.h>
#include <vfs.h>

int fseek(FILE *stream, long offset, int whence)
{
    switch (whence)
    {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            offset += pipe_get_flag(filepipe(stream), F_POSITION);
            break;
        case SEEK_END:
            offset += pipe_get_flag(filepipe(stream), F_FILESIZE);
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return pipe_set_flag(filepipe(stream), F_POSITION, offset) ? 0 : -1;
}
