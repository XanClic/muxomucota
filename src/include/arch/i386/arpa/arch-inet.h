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

#ifndef _ARPA__ARCH_INET_H
#define _ARPA__ARCH_INET_H

#include <stdint.h>

static inline uint32_t htonl(uint32_t hostlong)
{
    return __builtin_bswap32(hostlong);
}

static inline uint16_t htons(uint16_t hostshort)
{
    return ((hostshort & 0xff00) >> 8) | ((hostshort & 0x00ff) << 8);
}

static inline uint32_t ntohl(uint32_t netlong)
{
    return __builtin_bswap32(netlong);
}

static inline uint16_t ntohs(uint16_t netshort)
{
    return ((netshort & 0xff00) >> 8) | ((netshort & 0x00ff) << 8);
}

#endif
