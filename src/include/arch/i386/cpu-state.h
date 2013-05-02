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

#ifndef _CPU_STATE_H
#define _CPU_STATE_H

#include <compiler.h>
#include <stdint.h>


struct cpu_state
{
    uint32_t ebp, edi, esi, edx, ecx, ebx, eax;
    uint32_t es, ds;
    uint32_t int_vector, err_code;
    uint32_t eip, cs, eflags, esp, ss;
    uint32_t vm86_es, vm86_ds, vm86_fs, vm86_gs;
} cc_packed;

#endif
