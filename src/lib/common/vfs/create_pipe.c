#include <errno.h>
#include <ipc.h>
#include <lock.h>
#include <stdbool.h>
#include <string.h>
#include <vfs.h>


lock_t _pipe_allocation_lock;


static bool extract_path(const char *fullpath, char *service, char *relpath)
{
    if (!*fullpath)
        return false;

    bool expect_brackets = (*fullpath == '(');

    if (expect_brackets)
        fullpath++;

    while (*fullpath && (!expect_brackets || (*fullpath != ')')) && (expect_brackets || (*fullpath != ':')))
        *(service++) = *(fullpath++);
    *service = 0;

    if (!*(fullpath++))
        return false;

    if (!expect_brackets && (*(fullpath++) != '/'))
        return false;

    if (*fullpath != '/')
        return false;

    while (*fullpath)
        *(relpath++) = *(fullpath++);
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
