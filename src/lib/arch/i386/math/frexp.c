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

#include <math.h>
#include <stdint.h>


double frexp(double x, int *ex)
{
    uint64_t *dir = (uint64_t *)&x;

    *ex = ((*dir >> 52) & ((1 << 11) - 1)) - 1022;

    *dir &= ~(((1LL << 11) - 1) << 52);
    *dir |= 1022LL << 52;

    return x;
}


float frexpf(float x, int *ex)
{
    uint32_t *dir = (uint32_t *)&x;

    *ex = ((*dir >> 23) & ((1 << 8) - 1)) - 126;

    *dir &= ~(((1 << 8) - 1) << 23);
    *dir |= 126 << 23;

    return x;
}


long double frexpl(long double x, int *ex)
{
    // FIXME
    *ex = 0;

    while (fabsl(x) < .5L)
    {
        x *= 2.L;
        (*ex)--;
    }

    while (fabsl(x) >= 1.L)
    {
        x /= 2.L;
        (*ex)++;
    }

    return x;
}
