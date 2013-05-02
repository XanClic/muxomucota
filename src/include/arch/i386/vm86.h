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

#ifndef _VM86_H
#define _VM86_H

#include <cpu-state.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


struct vm86_registers
{
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint32_t ds, es, fs, gs;
};

struct vm86_memory_area
{
    uintptr_t vm;
    void *caller;
    size_t size;
    bool in, out;
};


// Returns 0 on success, -errno on error.
void vm86_interrupt(int intr, struct vm86_registers *regs, struct vm86_memory_area *mem, int areas);

#ifdef KERNEL
bool handle_vm86_gpf(struct cpu_state *state);
#endif

#endif
