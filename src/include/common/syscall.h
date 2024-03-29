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

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <cpu-state.h>
#include <stdint.h>

#ifndef KERNEL
#include <arch-syscall.h>
#endif


enum syscall_number
{
    SYS_EXIT, // 0
    SYS_MAP_MEMORY,
    SYS_UNMAP_MEMORY,
    SYS_DAEMONIZE,
    SYS_POPUP_ENTRY,
    SYS_POPUP_EXIT,
    SYS_IPC_POPUP,
    SYS_POPUP_GET_MESSAGE,
    SYS_FIND_DAEMON_BY_NAME, // 8
    SYS_SHM_CREATE,
    SYS_SHM_MAKE,
    SYS_SHM_OPEN,
    SYS_SHM_CLOSE,
    SYS_SHM_UNMAKE,
    SYS_SHM_SIZE,
    SYS_SBRK,
    SYS_YIELD, // 10
    SYS_FORK,
    SYS_EXEC,
    SYS_WAIT,
    SYS_HANDLE_IRQ,
    SYS_IRQ_HANDLER_EXIT,
    SYS_IOPL,
    SYS_PHYS_ALLOC,
    SYS_PHYS_FREE, // 18
    SYS_SLEEP,
    SYS_CREATE_THREAD,
    SYS_GETPGID,
    SYS_ELAPSED_MS,
    SYS_VM86,
    SYS_POPUP_SET_ANSWER,
    SYS_POPUP_GET_ANSWER,
    SYS_ADD_ALIAS, // 20
};


uintptr_t syscall_krn(int syscall_nr, uintptr_t p0, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, struct cpu_state *state);

#endif
