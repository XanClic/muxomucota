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

#ifndef _SYS__TYPES_H
#define _SYS__TYPES_H

#include <stddef.h>
#include <stdint.h>


typedef int_fast32_t id_t;

typedef int pid_t;
typedef int uid_t;
typedef int gid_t;

typedef  int32_t  blkcnt_t;
typedef uint32_t  blksize_t;
typedef     long  clock_t;
typedef uint32_t  clockid_t;
typedef uint64_t  dev_t;
typedef uint64_t  fsblkcnt_t;
typedef uint64_t  fsfilcnt_t;
typedef uint32_t  ino_t;
typedef uint32_t  mode_t;
typedef uint32_t  nlink_t;
typedef  int64_t  off_t;
typedef  int64_t  off64_t;
typedef  int64_t  suseconds_t;
typedef uint64_t  time_t;
typedef uint64_t  useconds_t;

typedef uint64_t  blkcnt64_t;
typedef uint64_t  blksize64_t;
typedef uint64_t  blknum64_t;

#endif
