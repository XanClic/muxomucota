#include <errno.h>
#include <stdlib.h>
#include <string.h>


extern char **environ;


int unsetenv(const char *name)
{
    if ((name == NULL) || !*name || (strchr(name, '=') != NULL))
    {
        errno = EINVAL;
        return -1;
    }

    putenv((char *)name);

    return 0;
}
