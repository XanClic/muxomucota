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

#include <kassert.h>
#include <cpu.h>
#include <ipc.h>
#include <isr.h>
#include <kmalloc.h>
#include <lock.h>
#include <process.h>
#include <stdbool.h>
#include <stddef.h>


extern process_t *dead, *daemons, *runqueue;


static void reaper(void)
{
    for (;;)
    {
        // Prozessleichen aufräumen
        if (dead != NULL)
            sweep_dead_processes();

        // IRQs bereitmachen, nachdem IRQ-Threads fertig geworden sind
        for (process_t *p = daemons; p != NULL; p = p->next)
            if ((p->handled_irq >= 0) && (p->settle_count > 0) && (p->status == PROCESS_DAEMON))
                do
                    irq_settled(p->handled_irq);
                while (__sync_sub_and_fetch(&p->settle_count, 1));

        yield();
    }
}


void enter_idle_process(void)
{
    run_process(create_kernel_thread("reaper", reaper));


    make_idle_process();

    for (;;)
    {
        bool runqueue_empty = true;

        for (process_t *proc = runqueue; proc != NULL; proc = proc->next)
        {
            if (proc->status == PROCESS_ACTIVE)
            {
                runqueue_empty = false;
                break;
            }
        }

        if (runqueue_empty)
            cpu_halt();
        else
            yield();
    }
}
