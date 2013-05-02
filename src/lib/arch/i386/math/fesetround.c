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

#include <errno.h>
#include <fenv.h>
#include <stdint.h>

int fesetround(int mode)
{
    if (mode & ~3)
    {
        errno = EINVAL;
        return -1;
    }

    uint16_t ctrl_word;

    __asm__ __volatile__ ("fstcw %0" : "=m"(ctrl_word));

    ctrl_word = (ctrl_word & ~(3 << 10)) | (mode << 10);

    __asm__ __volatile__ ("fldcw %0" :: "m"(ctrl_word));

    return 0;
}
