#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <shm.h>
#include <stdbool.h>
#include <string.h>
#include <vfs.h>


lock_t _pipe_allocation_lock;

extern char *_cwd;


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
                char realpath[strlen(_cwd) + strlen(fullpath) + 2];

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

        if (*fullpath != '/')
            return false;
    }

    while (*fullpath)
    {
        // TODO: Auch . und .. behandeln
        if ((fullpath[0] == '/') && (fullpath[1] == '/'))
            fullpath++;
        else
            *(relpath++) = *(fullpath++);
    }

    *relpath = 0;

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

            char procname[len + 1], relpath[len + 1 + sizeof(int)];

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


            _pipes[i].id = ipc_message_synced(_pipes[i].pid, CREATE_PIPE, ipc_cp, sizeof(int) + strlen(ipc_cp->relpath) + 1);

            if (!_pipes[i].id)
            {
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

                        shm_close(shmid, npath);

                        return fd;
                    }

                    shm_close(shmid, npath);
                }

                errno = ENOENT; // FIXME
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
