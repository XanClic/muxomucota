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

#ifndef _FLOAT_H
#define _FLOAT_H

#define FLT_ROUNDS      -1
#define FLT_EVAL_METHOD -1

#define DECIMAL_DIG 21

#define FLT_RADIX   2

#define FLT_MANT_DIG    24
#define FLT_EPSILON     (0x800001p-23f - 0x1p0f)
#define FLT_DIG         6
#define FLT_MIN_EXP     -125
#define FLT_MIN         0x1p-126f
#define FLT_MIN_10_EXP  -37
#define FLT_MAX_EXP     128
#define FLT_MAX         0xffffffp+104f
#define FLT_MAX_10_EXP  38
#define FLT_TRUE_MIN    0x1p-149f
#define FLT_DECIMAL_DIG 9
#define FLT_HAS_SUBNORM 1

#define DBL_MANT_DIG    53
#define DBL_EPSILON     (0x10000000000001p-52 - 0x1p0)
#define DBL_DIG         15
#define DBL_MIN_EXP     -1021
#define DBL_MIN         0x1p-1022
#define DBL_MIN_10_EXP  -307
#define DBL_MAX_EXP     1024
#define DBL_MAX         0x1fffffffffffffp+971
#define DBL_MAX_10_EXP  308
#define DBL_TRUE_MIN    0x1p-1074f
#define DBL_DECIMAL_DIG 17
#define DBL_HAS_SUBNORM 1

#define LDBL_MANT_DIG       64
#define LDBL_EPSILON        (0x8000000000000001p-63l - 0x1p0l)
#define LDBL_DIG            18
#define LDBL_MIN_EXP        -16381
#define LDBL_MIN            0x1p-16382l
#define LDBL_MIN_10_EXP     -4931
#define LDBL_MAX_EXP        16384
#define LDBL_MAX            0xffffffffffffffffp+16320l
#define LDBL_MAX_10_EXP     4932
#define LDBL_TRUE_MIN       0x1p-16445l
#define LDBL_DECIMAL_DIG    21
#define LDBL_HAS_SUBNORM    1

#endif
