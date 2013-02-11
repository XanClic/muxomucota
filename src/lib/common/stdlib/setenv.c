#include <errno.h>
#include <stdlib.h>
#include <string.h>


extern char **environ;


int setenv(const char *name, const char *value, int overwrite)
{
    size_t len;

    if ((name == NULL) || !*name || (strchr(name, '=') != NULL) || (value == NULL) || !*value)
    {
        errno = EINVAL;
        return -1;
    }

    len = strlen(name);

    int i;
    if (environ == NULL)
        i = 0;
    else
    {
        for (i = 0; environ[i] != NULL; i++)
        {
            if (!strncmp(environ[i], name, len) && (environ[i][len] == '='))
            {
                if (!overwrite)
                    return 0;

                // free() tut nicht, wenn irgendein Dödel putenv() benutzt.
                // Aber na ja. Ergo Memleak.
                environ[i] = malloc(len + strlen(value) + 2);
                strcpy(environ[i], name);
                strcat(environ[i], "=");
                strcat(environ[i], value);

                return 0;
            }
        }
    }

    char **env = realloc(environ, (i + 2) * sizeof(*environ));
    if (env == NULL)
        return -1;

    environ = env;
    environ[i] = malloc(len + strlen(value) + 2);
    strcpy(environ[i], name);
    strcat(environ[i], "=");
    strcat(environ[i], value);
    environ[i + 1] = NULL;

    return 0;
}
