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

#define __ldexp(name, type) \
    type name(type x, int expo) \
    { \
        type r; \
        type e = expo; \
        __asm__ __volatile__ ("fscale; fst %%st(0)" : "=t"(r) : "0"(x), "u"(e)); \
        return r; \
    }

__ldexp(ldexp, double)
__ldexp(ldexpf, float)
__ldexp(ldexpl, long double)
