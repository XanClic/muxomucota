#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


extern char *_cwd;


int chdir(const char *path)
{
    if ((path[0] != '/') && (path[0] != '('))
    {
        const char *colon = strchr(path, ':');
        const char *slash = strchr(path, '/');

        if ((colon == NULL) || (slash == NULL) || (colon + 1 != slash))
        {
            errno = ENOENT;
            return -1;
        }
    }


    // FIXME: Versuchen, path mit cwd="" zu öffnen


    free(_cwd);

    _cwd = strdup(path);

    setenv("_SYS_PWD", _cwd, 1);


    return 0;
}
