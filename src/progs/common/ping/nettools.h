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

#ifndef NETTOOLS_H
#define NETTOOLS_H

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn static inline void version(char *argv0, int exitcode)
{
    printf("%s is part of the paloxena3.5 nettools.\n", basename(argv0));
    printf("Copyright (c) 2010, 2013 Hanna Reitz.\n");
    printf("License: MIT <http://www.opensource.org/licenses/mit-license.php>.\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");

    exit(exitcode);
}

#endif
