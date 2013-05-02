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

#ifndef _DRIVERS__PORTS_H
#define _DRIVERS__PORTS_H

#include <stdint.h>


void iopl(int level);


#define ___in(bits) \
    static inline uint##bits##_t in##bits(uint16_t port) \
    { \
        uint##bits##_t res; \
        __asm__ __volatile__ ("in %1,%0" : "=a"(res) : "Nd"(port)); \
        return res; \
    }

___in(8)
___in(16)
___in(32)

#undef ___in

#define ___out(bits) \
    static inline void out##bits(uint16_t port, uint##bits##_t value) \
    { \
        __asm__ __volatile__ ("out %1,%0" :: "Nd"(port), "a"(value)); \
    }

___out(8)
___out(16)
___out(32)

#undef ___out

#endif
