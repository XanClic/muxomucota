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
#include <shm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <system-timer.h>
#include <unistd.h>
#include <vmem.h>
#include <vfs.h>
#include <sys/wait.h>


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
    const char *file = NULL, *mountpoint = NULL, *fs_type = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-t"))
            {
                fprintf(stderr, "Unknown parameter “%s”.\n", argv[i]);
                return 1;
            }

            if (i == argc - 1)
            {
                fprintf(stderr, "Parameter expected for “-t”.\n");
                return 1;
            }

            fs_type = argv[++i];
        }
        else if (!file)
            file = argv[i];
        else if (!mountpoint)
            mountpoint = argv[i];
        else
        {
            fprintf(stderr, "Unexpected parameter “%s”.\n", argv[i]);
            return 1;
        }
    }

    if (!file || !mountpoint)
    {
        fprintf(stderr, "Usage: mount <file> -t fs-type <mountpoint>\n");
        return 1;
    }

    if (!fs_type)
    {
        fprintf(stderr, "There is no FS type autodetection; the FS type has to be specified via “-t”.\n");
        fprintf(stderr, "In order to do a µxoµcota-specific mount, use “-t vfs”.\n");
        return 1;
    }

    // TODO: Überprüfen, dass nur eine der beiden folgenden Möglichkeiten zutrifft:
    //  - fs-type ≠ "vfs" und mountpoint ist auf (root) (oder /)
    //  - fs-type = "vfs" und mountpoint-Service ist explizit gegeben und nicht (root)


    if (strcmp(fs_type, "vfs"))
    {
        char *temp_mp;

        for (;;)
        {
            asprintf(&temp_mp, "(%s)/_mount%i", fs_type, elapsed_ms());

            int fd = create_pipe(temp_mp, O_JUST_STAT);

            if (fd >= 0)
                destroy_pipe(fd, 0);
            else
                break;
        }


        pid_t child;

        if (!(child = fork()))
        {
            printf("mount '%s' '%s'\n", file, temp_mp);
            execlp("mount", "mount", file, temp_mp, NULL);
        }

        waitpid(child, NULL, 0);

        if (!(child = fork()))
        {
            printf("mount '%s' '%s'\n", temp_mp, mountpoint);
            execlp("mount", "mount", temp_mp, mountpoint, NULL);
        }

        waitpid(child, NULL, 0);


        return 0;
    }


    int fd = create_pipe(mountpoint, O_CREAT_MOUNT_POINT);

    if (fd < 0)
    {
        perror("Could not create mount point");
        return 1;
    }

    if (!pipe_implements(fd, I_FS))
    {
        fprintf(stderr, "Target directory does not implement the filesystem interface.\n");
        return 1;
    }


    big_size_t ret;

    if (is_absolute_path(file))
        ret = stream_send(fd, file, strlen(file) + 1, F_MOUNT_FILE);
    else
    {
        size_t fplen = strlen(_cwd) + 1 + strlen(file) + 1;

        char full_path[fplen];
        strcpy(full_path, _cwd);
        strcat(full_path, "/");
        strcat(full_path, file);

        ret = stream_send(fd, full_path, fplen, F_MOUNT_FILE);
    }

    if (!ret)
        fprintf(stderr, "Could not mount.\n");

    return !ret;
}
