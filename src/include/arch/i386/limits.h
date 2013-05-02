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

#ifndef _LIMITS_H
#define _LIMITS_H

#define  CHAR_BIT    8
#define SCHAR_MIN   -0x80
#define SCHAR_MAX    0x7F
#define UCHAR_MAX    0xFF
#define  CHAR_MIN    SCHAR_MIN
#define  CHAR_MAX    SCHAR_MAX

#define  SHRT_MIN   -0x8000
#define  SHRT_MAX    0x7FFF
#define USHRT_MAX    0xFFFF

#define  WORD_BIT    32
#define  INT_MIN   (-0x7FFFFFFF - 1)
#define  INT_MAX     0x7FFFFFFF
#define UINT_MAX     0xFFFFFFFFU

#define  LONG_BIT    32
#define  LONG_MIN  (-0x7FFFFFFFL - 1L)
#define  LONG_MAX    0x7FFFFFFFL
#define ULONG_MAX    0xFFFFFFFFUL

#define  LLONG_MIN (-0x7FFFFFFFFFFFFFFFLL - 1LL)
#define  LLONG_MAX   0x7FFFFFFFFFFFFFFFLL
#define ULLONG_MAX   0xFFFFFFFFFFFFFFFFULL

#define  SIZE_MAX   ULONG_MAX
#define SSIZE_MAX    LONG_MAX

#define MB_LEN_MAX   4

#endif
