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
#include <string.h>
#include <vfs.h>


static inline size_t min(size_t a, size_t b)
{
    return a < b ? a : b;
}


static void cat(int fd)
{
    bool file = pipe_implements(fd, I_FILE);

    while (pipe_get_flag(fd, F_READABLE)) {
        size_t press = min(65536, pipe_get_flag(fd, F_PRESSURE));

        if (!press && file) {
            break;
        }

        char inp[press];
        stream_recv(fd, inp, press, 0);
        stream_send(filepipe(stdout), inp, press, 0);
    }
}


int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        int fd = filepipe(stdin);
        cat(fd);

        return 0;
    }


    for (int i = 1; i < argc; i++)
    {
        int fd = create_pipe(argv[i], O_RDONLY);

        if (fd < 0)
        {
            fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
            continue;
        }

        cat(fd);
    }


    return 0;
}
