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

#ifndef _X86_SEGMENTS_H
#define _X86_SEGMENTS_H

#include <compiler.h>
#include <stdint.h>

#define SEG_SYS_CS  0x0008
#define SEG_SYS_DS  0x0010
#define SEG_USR_CS  0x001B
#define SEG_USR_DS  0x0023
#define SEG_USR_TLS 0x0033

struct tss
{
    uint32_t prev_task;

    uint32_t esp0, ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;

    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;

    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;

    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;

    uint16_t rsvd;
    uint16_t io_map_base;
} cc_packed;

#endif
