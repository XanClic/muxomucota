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

#ifndef _ISR_H
#define _ISR_H

#include <cpu-state.h>
#include <process.h>


struct cpu_state;


void init_interrupts(void);

int common_irq_handler(int irq, struct cpu_state *state);

void irq_settled(int irq);

void register_kernel_isr(int irq, void (*isr)(struct cpu_state *state));
void register_user_isr(int irq, process_t *process, void (__attribute__((regparm(1))) *process_handler)(void *info), void *info);
void unregister_isr_process(process_t *process);

#endif
