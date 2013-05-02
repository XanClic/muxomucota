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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>
#include <sys/stat.h>


extern char *_cwd;


int chdir(const char *path)
{
    char *oldcwd = _cwd;

    _cwd = NULL;


    int fd = create_pipe(path, O_JUST_STAT);
    if (fd < 0)
    {
        _cwd = oldcwd;
        return -1;
    }

    _cwd = oldcwd;

    if (!pipe_implements(fd, I_STATABLE))
    {
        destroy_pipe(fd, 0);
        errno = ENOTSUP;
        return -1;
    }

    if (!S_ISDIR(pipe_get_flag(fd, F_MODE)))
    {
        destroy_pipe(fd, 0);
        errno = ENOTDIR;
        return -1;
    }


    destroy_pipe(fd, 0);


    free(_cwd);

    _cwd = strdup(path);

    setenv("_SYS_PWD", _cwd, 1);


    return 0;
}
