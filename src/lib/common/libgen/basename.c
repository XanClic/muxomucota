#include <libgen.h>
#include <string.h>

char *basename(char *path)
{
    char *last_slash = NULL;
    size_t pathlen;

    for (pathlen = 0; path[pathlen]; pathlen++)
        if (path[pathlen] == '/')
            last_slash = &path[pathlen + 1];

    if (last_slash == NULL)
        return path;

    if (!*last_slash)
    {
        if (pathlen <= 1)
            return path; // "/" oder ""

        last_slash[-1] = 0;
        return basename(path);
    }

    return last_slash;
}
