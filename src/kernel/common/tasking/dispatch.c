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
#include <lock.h>
#include <process.h>
#include <vmem.h>


extern process_t *idle_process;

static lock_t dispatch_lock = LOCK_INITIALIZER;


struct cpu_state *dispatch(struct cpu_state *state, pid_t switch_to)
{
    if (unlikely(!trylock(&dispatch_lock)))
        return state;

    if (likely(current_process != NULL))
        current_process->cpu_state = state;
    else if (likely(idle_process != NULL))
        idle_process->cpu_state = state;


    process_t *next_process = schedule(switch_to);

    if (unlikely(next_process == NULL))
    {
        unlock(&dispatch_lock);
        return state;
    }


    change_vmm_context(next_process->vmmc);

    arch_process_change(next_process);


    unlock(&dispatch_lock);

    return next_process->cpu_state;
}
