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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void *memmove(void *d, const void *s, size_t n)
{
    const uint8_t *s8 = s;
    uint8_t *d8 = d;

    if (((uintptr_t)d < (uintptr_t)s) || ((uintptr_t)s + n <= (uintptr_t)d))
        while (n--)
            *(d8++) = *(s8++);
    else
        while (n--)
            d8[n] = s8[n];

    return d;
}
