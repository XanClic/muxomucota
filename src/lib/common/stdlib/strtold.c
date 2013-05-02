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

#include <ctype.h>
#include <stdlib.h>

long double strtold(const char *nptr, char **endptr)
{
    long double val = 0;
    int in_dec = 0;
    long double dec_multiplier = 0.1;

    while (*nptr)
    {
        if (isdigit(*nptr))
        {
            if (!in_dec)
            {
                val *= 10;
                val += *nptr - '0';
            }
            else
            {
                val += (long double)(*nptr - '0') * dec_multiplier;
                dec_multiplier /= 10.0;
            }
        }
        else if (*nptr == '.')
        {
            if (in_dec)
                break;
            in_dec = 1;
        }
        else
            break;
        // TODO: Exp

        nptr++;
    }

    if (endptr != NULL)
        *endptr = (char *)nptr;

    return val;
}
