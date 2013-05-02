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

long double powl(long double x, long double y)
{
    long double res;

    __asm__ __volatile__ (
        // ST(0) == x; ST(1) == y
        "fld1;"
        // ST(0) == 1; ST(1) == x; ST(2) == y
        "fxch;"
        // ST(0) == x; ST(1) == 1; ST(2) == y
        "fyl2x;"
        // ST(0) == log2(x); ST(1) == y
        "fmulp;"
        // ST(0) == y * log2(x)
        "f2xm1;"
        // ST(0) == 2^(y * log2(x)) - 1 == x^y - 1
        "fld1;"
        // ST(0) == 1; ST(1) == x^y - 1
        "faddp"
        // ST(0) == x^y
        : "=t"(res) : "0"(x), "u"(y));

    return res;
}

double pow(double x, double y)
{
    return powl(x, y);
}

float powf(float x, float y)
{
    return powf(x, y);
}
