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
