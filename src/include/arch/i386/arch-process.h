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

#ifndef _ARCH_PROCESS_H
#define _ARCH_PROCESS_H

#include <compiler.h>
#include <stdbool.h>
#include <stdint.h>
#include <vm86.h>


struct fxsave_space
{
    uint16_t fcw, fsw;
    uint8_t ftw;
    uint8_t rsvd0;
    uint16_t fop;
    uint32_t fpu_ip;
    uint16_t cs;
    uint16_t rsvd1;
    uint32_t fpu_dp;
    uint16_t ds;
    uint16_t rsvd2;
    uint32_t mxcsr, mxcsr_mask;

    struct
    {
        union
        {
            uint8_t st[10];
            uint8_t mm[10];
        };
        uint8_t rsvd3[6];
    } cc_packed st[8];

    struct
    {
        uint8_t bytes[16];
    } cc_packed xmm[8];

    uint8_t rsvd4[11 * 16];
    uint8_t avail[3 * 16];
} cc_packed;

struct process_arch_info
{
    uintptr_t kernel_stack, kernel_stack_top;
    int iopl;

    bool fxsave_valid;
    struct fxsave_space *fxsave;

    uint8_t fxsave_real_space[527];

    struct vm86_registers *vm86_exit_regs;
};


void save_and_restore_fpu_and_sse(void);

struct process;
void fpu_sse_unregister(struct process *proc);

#endif
