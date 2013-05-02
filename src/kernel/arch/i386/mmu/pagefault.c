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

#include <cpu-state.h>
#include <kassert.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <vmem.h>


bool handle_pagefault(struct cpu_state *state);

bool handle_pagefault(struct cpu_state *state)
{
    if (current_process == NULL)
        return false;


    uintptr_t cr2;

    __asm__ __volatile__ ("mov %0,%%cr2" : "=r"(cr2));

    // Schreibzugriff, könnte COW sein
    if ((state->err_code & (1 << 1)) && vmmc_do_cow(current_process->vmmc, (void *)cr2))
        return true;

    return vmmc_do_lazy_map(current_process->vmmc, (void *)cr2);
}
