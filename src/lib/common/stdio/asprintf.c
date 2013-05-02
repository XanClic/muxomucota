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
#include <stdio.h>
#include <stdlib.h>

int asprintf(char **strp, const char *format, ...)
{
    va_list args1, args2;
    int rval;

    va_start(args1, format);
    va_copy(args2, args1);
    rval = vsnprintf(NULL, 0, format, args1);
    va_end(args1);

    *strp = malloc(rval + 1);
    vsnprintf(*strp, rval + 1, format, args2);
    va_end(args2);

    return rval;
}
