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

#include <compiler.h>
#include <ipc.h>
#include <shm.h>
#include <stdbool.h>
#include <stddef.h>
#include <vfs.h>


uintmax_t _vfs_is_symlink(uintptr_t shmid);

uintmax_t _vfs_is_symlink(uintptr_t shmid)
{
    size_t pathlen = popup_get_message(NULL, 0);

    char path[pathlen];
    popup_get_message(path, pathlen);


    void *dst = shm_open(shmid, VMM_UW);

    bool is_link = service_is_symlink(path, dst);

    shm_close(shmid, dst, true);


    return is_link;
}


bool service_is_symlink(const char *relpath, char *linkpath) cc_weak;

bool service_is_symlink(const char *relpath, char *linkpath)
{
    (void)relpath;
    (void)linkpath;

    return false;
}
