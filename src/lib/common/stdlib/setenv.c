/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

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
