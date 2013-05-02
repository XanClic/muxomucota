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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vfs.h>


FILE *stdin  = &(FILE){ .fd = STDIN_FILENO  };
FILE *stdout = &(FILE){ .fd = STDOUT_FILENO };
FILE *stderr = &(FILE){ .fd = STDERR_FILENO };


void _vfs_init(void);

void _vfs_init(void)
{
}


void _vfs_deinit(void);

void _vfs_deinit(void)
{
    for (int i = 0; i < __MFILE; i++)
        destroy_pipe(i, 0);
}
