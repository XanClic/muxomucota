#include <stdlib.h>
#include <string.h>


extern char **environ;


char *getenv(const char *name)
{
    size_t len = strlen(name);

    for (int i = 0; environ[i] != NULL; i++)
        if (!strncmp(environ[i], name, len) && (environ[i][len] == '='))
            return &environ[i][len + 1];

    return NULL;
}
