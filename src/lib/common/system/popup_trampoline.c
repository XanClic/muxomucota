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
#include <stdint.h>
#include <stddef.h>


uintmax_t (*popup_entries[MAX_POPUP_HANDLERS])(uintptr_t);


inline void _popup_trampoline(int func_index, uintptr_t shmid);


void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid) __attribute__((weak));

void _popup_ll_trampoline(uintptr_t func_index, uintptr_t shmid)
{
    _popup_trampoline((int)func_index, shmid);
}


void _popup_trampoline(int func_index, uintptr_t shmid)
{
    uintmax_t retval = 0;

    if (popup_entries[func_index] != NULL)
        retval = popup_entries[func_index](shmid);

    popup_exit(retval);
}
