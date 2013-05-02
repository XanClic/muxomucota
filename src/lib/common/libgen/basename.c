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
