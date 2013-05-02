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
