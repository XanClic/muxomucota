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

#include <shm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vmem.h>
#include <vfs.h>


extern char *_cwd; // FIXME


static bool is_absolute_path(const char *path)
{
    if ((path[0] == '/') || (path[0] == '('))
        return true;

    const char *colon = strchr(path, ':');
    return (colon != NULL) && (colon[1] == '/');
}


int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: mount <file> <mountpoint>\n");
        return 1;
    }

    int fd = create_pipe(argv[2], O_CREAT_MOUNT_POINT);

    if (fd < 0)
    {
        perror("Could not open file");
        return 1;
    }

    if (!pipe_implements(fd, I_FS))
    {
        destroy_pipe(fd, 0);
        fprintf(stderr, "Target directory does not implement the filesystem interface.\n");
        return 1;
    }


    big_size_t ret;

    if (is_absolute_path(argv[1]))
        ret = stream_send(fd, argv[1], strlen(argv[1]) + 1, F_MOUNT_FILE);
    else
    {
        size_t fplen = strlen(_cwd) + 1 + strlen(argv[1]) + 1;

        char full_path[fplen];
        strcpy(full_path, _cwd);
        strcat(full_path, "/");
        strcat(full_path, argv[1]);

        ret = stream_send(fd, full_path, fplen, F_MOUNT_FILE);
    }

    destroy_pipe(fd, 0);


    if (!ret)
        fprintf(stderr, "Could not mount.\n");

    return !ret;
}
