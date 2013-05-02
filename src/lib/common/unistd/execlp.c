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

#include <stdarg.h>
#include <unistd.h>


int execlp(const char *file, const char *arg, ...)
{
    va_list va1, va2;

    va_start(va1, arg);
    va_copy(va2, va1);

    int arg_count = 1;

    while (va_arg(va1, const char *) != NULL)
        arg_count++;

    va_end(va1);


    const char *argv[arg_count + 1];

    argv[0] = arg;

    for (int i = 1; argv[i - 1] != NULL; i++)
        argv[i] = va_arg(va2, const char *);

    va_end(va2);


    return execvp(file, (char *const *)argv);
}
