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

double asin(double x)
{
    if ((x > 1.) || (x < -1.))
        return __domain_error();
    if (isnan(x))
        return NAN;
    return 2. * atan(x / (1. + sqrt(1. - x * x)));
}

float asinf(float x)
{
    if ((x > 1.f) || (x < -1.f))
        return __domain_error();
    if (isnan(x))
        return NAN;
    return 2.f * atanf(x / (1.f + sqrtf(1.f - x * x)));
}

long double asinl(long double x)
{
    if ((x > 1.l) || (x < -1.l))
        return __domain_error();
    if (isnan(x))
        return NAN;
    return 2.l * atanl(x / (1.l + sqrtl(1.l - x * x)));
}
