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
#include <isr.h>
#include <kassert.h>
#include <port-io.h>
#include <process.h>
#include <system-timer.h>


volatile int elapsed_ms = 0;


static void timer_isr(struct cpu_state *state);


void init_system_timer(void)
{
    int multiplier = 1193182 / SYSTEM_TIMER_FREQUENCY;

    register_kernel_isr(0, timer_isr);

    out8(0x43, 0x34);
    out8(0x40, multiplier & 0xFF);
    out8(0x40, multiplier >> 8);
}


static void timer_isr(struct cpu_state *state)
{
    elapsed_ms += 1000 / SYSTEM_TIMER_FREQUENCY;

#ifdef COOPERATIVE
    (void)state;
#else
    // Den Kernelspace müssen wir nicht präemptieren. Hoffentlich.
    if (state->cs & 3)
        yield();
#endif
}


void sleep(int ms)
{
    int end = elapsed_ms + ms;

    while (elapsed_ms < end)
        yield();
}
