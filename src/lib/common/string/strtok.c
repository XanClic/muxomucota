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

#include <string.h>

char *strtok_r(char *string1, const char *string2, char **saveptr)
{
    char *tok;
    int slen = strlen(string2);

    if (string1 == NULL)
        string1 = *saveptr;

    if (string1 == NULL)
        return NULL;

    while ((tok = strstr(string1, string2)) == string1)
        string1 += slen;

    if (!*string1)
    {
        *saveptr = NULL;
        return NULL;
    }

    if (tok == NULL)
        *saveptr = NULL;
    else
    {
        *tok = '\0';
        *saveptr = tok + slen;
    }

    return string1;
}

char *strtok(char *string1, const char *string2)
{
    static char *next_token = NULL;

    return strtok_r(string1, string2, &next_token);
}
