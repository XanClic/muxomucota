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
#include <shm.h>
#include <stdbool.h>
#include <string.h>
#include <vfs.h>


lock_t _pipe_allocation_lock;

extern char *_cwd;


void __simplify_path(char *path);

void __simplify_path(char *path)
{
    size_t len = strlen(path) + 1;

    int changed_anything = 0;

    for (char *part = strchr(path, '/'); part != NULL; part = strchr(part + !changed_anything, '/'))
    {
        changed_anything = 0;

        while (part[1] == '/')
        {
            memmove(&part[1], &part[2], &path[len] - part - 2);
            changed_anything = 1;
        }

        if (!part[1])
        {
            part[0] = 0;
            break;
        }

        while ((part[1] == '.') && (part[2] == '/'))
        {
            memmove(&part[1], &part[3], &path[len] - part - 3);
            changed_anything = 1;
        }

        if ((part[1] == '.') && !part[2])
        {
            part[0] = 0;
            break;
        }

        while ((part[1] == '.') && (part[2] == '.') && ((part[3] == '/') || !part[3]))
        {
            char *prev = part - 1;
            while ((prev > path) && (*prev != '/'))
                prev--;

            if (*prev != '/')
                break;

            changed_anything = 1;

            if (part[3])
            {
                memmove(&prev[1], &part[4], &path[len] - part - 4);
                part = prev;
            }
            else
            {
                prev[0] = 0;
                part = prev;
                break;
            }
        }
    }
}


static bool extract_path(const char *fullpath, char *service, char *relpath)
{
    if (!*fullpath)
        return false;

    if (*fullpath == '/')
        strcpy(service, "root");
    else
    {
        bool expect_brackets = (*fullpath == '(');

        if (expect_brackets)
            fullpath++;

        if (!expect_brackets)
        {
            const char *colon = strchr(fullpath, ':');
            const char *slash = strchr(fullpath, '/');

            if ((colon == NULL) || (slash == NULL) || (colon + 1 != slash))
            {
                if (_cwd == NULL)
                    return false;

                char realpath[strlen(_cwd) + 1 + strlen(fullpath) + 1];

                strcpy(realpath, _cwd);
                strcat(realpath, "/");
                strcat(realpath, fullpath);

                return extract_path(realpath, service, relpath);
            }
        }

        while (*fullpath && (!expect_brackets || (*fullpath != ')')) && (expect_brackets || (*fullpath != ':')))
            *(service++) = *(fullpath++);
        *service = 0;

        if (!*(fullpath++))
            return false;

        if (!expect_brackets && (*(fullpath++) != '/'))
            return false;

        if (*fullpath && (*fullpath != '/'))
            return false;
    }

    strcpy(relpath, fullpath);

    __simplify_path(relpath);

    return true;
}


int create_pipe(const char *path, int flags)
{
    for (int i = 0; i < __MFILE; i++)
    {
        if (!_pipes[i].pid)
        {
            lock(&_pipe_allocation_lock);

            if (_pipes[i].pid)
                continue;

            // Erstmal als belegt markieren
            _pipes[i].pid = 1;

            unlock(&_pipe_allocation_lock);


            size_t len = strlen(path);

            if (len < 4) // Platz für (root) in procname
                len = 4;

            // evtl. muss da auch das CWD rein
            size_t relpath_maxlen = ((_cwd == NULL) ? 0 : (strlen(_cwd) + 1)) + len + 1 + sizeof(int);

            char procname[len + 1], relpath[relpath_maxlen];

            // FIXME
            struct ipc_create_pipe *ipc_cp = (struct ipc_create_pipe *)relpath;

            ipc_cp->flags = flags;


            if (!extract_path(path, procname, ipc_cp->relpath))
            {
                errno = EINVAL;
                goto error_after_allocation;
            }


            _pipes[i].pid = find_daemon_by_name(procname);

            if (_pipes[i].pid < 0)
            {
                errno = ESRCH;
                goto error_after_allocation;
            }


            if (!(flags & O_NOFOLLOW))
            {
                uintptr_t shmid = shm_create(4096);
                char *npath = shm_open(shmid, VMM_UR);

                if (ipc_shm_message_synced(_pipes[i].pid, IS_SYMLINK, shmid, ipc_cp->relpath, strlen(ipc_cp->relpath) + 1))
                {
                    lock(&_pipe_allocation_lock);
                    _pipes[i].pid = 0;
                    unlock(&_pipe_allocation_lock);

                    // FIXME: ELOOP
                    int fd = create_pipe(npath, flags);

                    shm_close(shmid, npath, true);

                    return fd;
                }

                shm_close(shmid, npath, true);
            }

            _pipes[i].id = ipc_message_synced(_pipes[i].pid, CREATE_PIPE, ipc_cp, sizeof(int) + strlen(ipc_cp->relpath) + 1);
            if (!_pipes[i].id) {
                goto error_after_allocation;
            }

            return i;


error_after_allocation:
            lock(&_pipe_allocation_lock);
            _pipes[i].pid = 0;
            unlock(&_pipe_allocation_lock);

            return -1;
        }
    }


    errno = EMFILE;
    return -1;
}
