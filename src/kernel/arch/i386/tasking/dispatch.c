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
#include <ipc.h>
#include <lock.h>
#include <process.h>
#include <tls.h>
#include <x86-segments.h>


extern struct tss x86_tss;

static process_t *last_fpu_sse_user = NULL;
static lock_t last_fpu_sse_user_lock = LOCK_INITIALIZER;


void arch_process_change(process_t *new_process)
{
    x86_tss.esp0 = new_process->arch.kernel_stack_top;

    set_tls(new_process->tls);

    __asm__ __volatile__ ("mov eax,cr0; or eax,8; mov cr0,eax" ::: "eax"); // TS setzen
}


void save_and_restore_fpu_and_sse(void)
{
    __asm__ __volatile__ ("mov eax,cr0; and eax,~8; mov cr0,eax" ::: "eax"); // TS löschen


    lock(&last_fpu_sse_user_lock);

    if (last_fpu_sse_user != NULL)
    {
        __asm__ __volatile__ ("fxsave %0" :: "m"(*last_fpu_sse_user->arch.fxsave));
        last_fpu_sse_user->arch.fxsave_valid = true;
    }

    last_fpu_sse_user = current_process;

    unlock(&last_fpu_sse_user_lock);


    if (current_process->arch.fxsave_valid)
        __asm__ __volatile__ ("fxrstor %0" :: "m"(*current_process->arch.fxsave));
    else
        __asm__ __volatile__ ("finit");
}


void fpu_sse_unregister(struct process *proc)
{
    lock(&last_fpu_sse_user_lock);

    if (last_fpu_sse_user == proc)
        last_fpu_sse_user = NULL;

    unlock(&last_fpu_sse_user_lock);
}
