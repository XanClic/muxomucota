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
#include <lock.h>
#include <unistd.h>


void rwl_lock_r(volatile rw_lock_t *rwl)
{
    lock(&rwl->lock_lock);

    if (!rwl->reader_count++)
        lock(&rwl->write_lock);

    unlock(&rwl->lock_lock);
}


void rwl_unlock_r(volatile rw_lock_t *rwl)
{
    lock(&rwl->lock_lock);

    if (!--rwl->reader_count)
        unlock(&rwl->write_lock);
    else if (rwl->write_lock == getpid())
        rwl->write_lock = -1;

    unlock(&rwl->lock_lock);
}


void rwl_lock_w(volatile rw_lock_t *rwl)
{
    lock(&rwl->write_lock);
}


void rwl_unlock_w(volatile rw_lock_t *rwl)
{
    unlock(&rwl->write_lock);
}


void rwl_require_w(volatile rw_lock_t *rwl)
{
    for (;;)
    {
        while ((rwl->reader_count > 1) || !trylock(&rwl->lock_lock))
            yield_to(rwl->write_lock);

        if (rwl->reader_count == 1)
            break;

        unlock(&rwl->lock_lock);
    }

    rwl->reader_count = 0;

    unlock(&rwl->lock_lock);
}


void rwl_drop_w(volatile rw_lock_t *rwl)
{
    rwl->reader_count = 1;
}
