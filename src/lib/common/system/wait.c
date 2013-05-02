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

#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/wait.h>


pid_t wait(int *stat_loc)
{
    return waitpid(-1, stat_loc, 0);
}


pid_t waitpid(pid_t pid, int *stat_loc, int options)
{
    uintmax_t exitval;

    pid_t retval = syscall3(SYS_WAIT, pid, (uintptr_t)&exitval, options);

    if ((stat_loc != NULL) && (retval > 0))
        *stat_loc = (int)exitval;

    return retval;
}
