#include <libgen.h>
#include <string.h>

char *dirname(char *path)
{
    char *last_slash = NULL;
    size_t pathlen;

    for (pathlen = 0; path[pathlen]; pathlen++)
        if ((path[pathlen] == '/') && path[pathlen + 1])
            last_slash = &path[pathlen];

    if (last_slash == NULL)
    {
        static char tmp[2] = { 0 };
        tmp[0] = (path[0] == '/') ? '/' : '.';
        return tmp;
    }

    last_slash[last_slash == path] = 0; // "/" statt "" zurückgeben

    return path;
}
