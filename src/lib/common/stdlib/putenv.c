#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


extern char **environ;


int putenv(char *string)
{
    bool free_var = false;

    size_t len;
    char *equal_sign = strchr(string, '=');

    if (equal_sign != NULL)
        len = equal_sign - string;
    else
    {
        len = strlen(string);
        free_var = true;
    }

    int i;
    if (environ == NULL)
        i = 0;
    else
    {
        for (i = 0; environ[i] != NULL; i++)
        {
            if (!strncmp(environ[i], string, len) && (environ[i][len] == '='))
            {
                if (!free_var)
                    environ[i] = string;
                else
                {
                    int j;
                    for (j = i + 1; environ[j] != NULL; j++);
                    // free() geht nicht, weil putenv beschissen ist. Wenn die
                    // Variable hier mit putenv gesetzt wurde, muss sie nicht
                    // auf dem Heap liegen.
                    environ[i] = environ[j - 1];
                    environ[j - 1] = NULL;
                }

                return 0;
            }
        }
    }

    if (free_var)
        return 0;

    char **env = realloc(environ, (i + 2) * sizeof(*environ));
    if (env == NULL)
        return -1;

    environ = env;
    environ[i] = string;
    environ[i + 1] = NULL;

    return 0;
}
