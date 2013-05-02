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
#include <syscall.h>


extern uintmax_t (*popup_entries[])(uintptr_t);


void popup_message_handler(int index, uintmax_t (*handler)(void))
{
    // FIXME: Überlauf und so
    popup_entries[index] = (uintmax_t (*)(uintptr_t))handler;
}


void popup_shm_handler(int index, uintmax_t (*handler)(uintptr_t))
{
    popup_entries[index] = handler;
}


void popup_ping_handler(int index, uintmax_t (*handler)(void))
{
    popup_entries[index] = (uintmax_t (*)(uintptr_t))handler;
}
