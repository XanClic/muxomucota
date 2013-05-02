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

#include <ipc.h>
#include <kassert.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


void lock(volatile lock_t *v)
{
    pid_t mypid = (current_process == NULL) ? -1 : current_process->pid, owner;

    while ((owner = __sync_val_compare_and_swap(v, 0, mypid)) != 0)
    {
        kassert(mypid != owner);
        yield_to(owner);
    }
}


void unlock(volatile lock_t *v)
{
    __sync_lock_release(v);
}


bool trylock(volatile lock_t *v)
{
    return __sync_bool_compare_and_swap(v, 0, (current_process == NULL) ? -1 : current_process->pid);
}
