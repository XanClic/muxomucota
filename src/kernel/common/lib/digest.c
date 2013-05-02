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

#include <digest.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


static digest_t crc32(const void *buffer, size_t size)
{
    digest_t rem_poly = 0xffffffffu;
    const uint8_t *bp = buffer;

    while (size--)
    {
#ifdef LITTLE_ENDIAN
        rem_poly ^= *(bp++);

        for (int i = 0; i < 8; i++)
            if (rem_poly & 1)
                rem_poly = (rem_poly >> 1) ^ 0xedb88320;
            else
                rem_poly >>= 1;
#else
#error Missing non-little-endian implementation for digest.c
#endif
    }

    return rem_poly ^ 0xffffffff;
}


digest_t _calculate_checksum(const void *buffer, size_t size)
{
    return crc32(buffer, size);
}


bool _check_integrity(const void *buffer, size_t size)
{
    return crc32(buffer, size) == 0x2144df1c;
}
