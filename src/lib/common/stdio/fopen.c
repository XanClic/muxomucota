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
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <vfs.h>

FILE *fopen(const char *path, const char *mode)
{
    int imode = 0;

    switch (mode[0])
    {
        case 'r':
            imode = (mode[1] == '+') ? O_RDWR : O_RDONLY;
            break;
        case 'w':
            imode = (mode[1] == '+') ? O_RDWR | O_CREAT | O_TRUNC : O_WRONLY | O_CREAT | O_TRUNC;
            break;
        case 'a':
            imode = (mode[1] == '+') ? O_RDWR | O_APPEND : O_WRONLY | O_APPEND;
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    int fd = create_pipe(path, imode);
    if (fd < 0)
        return NULL;

    return fdopen(fd, mode);
}
